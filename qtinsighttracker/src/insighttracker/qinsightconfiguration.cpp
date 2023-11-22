// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial

#include <QtInsightTracker/QInsightConfiguration>
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QLibraryInfo>
#include <QtCore/QLoggingCategory>
#include <QtCore/QStandardPaths>
#include <chrono>

namespace {

static const QLatin1String DEFAULT_CONF_FILE("qtinsight.conf");

const QSet<QString> SNOWPLOW_PLATFORMS = {
    QStringLiteral("web"), QStringLiteral("mob"), QStringLiteral("pc"), QStringLiteral("srv"),
    QStringLiteral("app"), QStringLiteral("tv"), QStringLiteral("cnsl"), QStringLiteral("iot")};

qint64 intervalToSec(const QJsonObject &interval)
{
    int seconds = interval.value(QLatin1String("seconds")).toInt();
    int minutes = interval.value(QLatin1String("minutes")).toInt();
    int hours = interval.value(QLatin1String("hours")).toInt();
    int days = interval.value(QLatin1String("days")).toInt();
    int months = interval.value(QLatin1String("months")).toInt();

    auto duration = std::chrono::seconds(seconds) +
            std::chrono::minutes(minutes) +
            std::chrono::hours(hours);

    // No days and months in C++17 yet :(
    duration += std::chrono::hours(days * 24) +
            std::chrono::hours(months * 30 * 24);

    return std::chrono::duration_cast<std::chrono::seconds>(duration).count();
}

struct Configuration
{
    QString m_server = QString();
    QString m_token = QString();
    QString m_deviceModel = QString();
    QString m_deviceVariant = QString();
    QString m_deviceScreenType = QString();
    QString m_appBuild = QString();
    QString m_platform = QString();
    QString m_storageType = QLatin1String("SQLITE");
    QString m_storagePath = QString();
    int m_storageSize = 0;
    int m_syncInterval = 0;
    int m_batchSize = 100;
    QStringList m_categories = {};
    QStringList m_events = {};
};

Configuration &configuration()
{
    static Configuration configuration;
    return configuration;
}

} // unnamed namespace

Q_LOGGING_CATEGORY(lcQInsightConfig, "qt.insight.config", QtWarningMsg)

/*!
    \class QInsightConfiguration
    \inmodule QtInsightTracker
    \brief Configuration class for the Qt Insight Tracker.

    Use QInsightConfiguration to configure the tracker. You must make all
    configuration changes before enabling the tracking. The tracker reads
    default values from a configuration file (\c qtinsight.conf), which is
    searched from following the locations in a decending order:
    \list 1
    \li Application's resources file.
    \li Application's directory.
    \li Current working directory.
    \li System configuration directories as defined by \a QStandardPaths::GenericConfigLocation.
    \endlist

    Use an environment variable \c QT_INSIGHT_CONFIG to override the used
    configuration file.

    Example of a JSON configuration file:
    \code
    {
        "server" : "collect-insight.qt.io",
        "token" : "00000000-0000-0000-0000-000000000000",
        "device_model" :  "model 1",
        "device_variant" : "a",
        "device_screen_type" : "NON_TOUCH",
        "platform" : "app",
        "app_build" : "1.2.3",
        "storage" : "SQLITE",
        "storage_path" : "",
        "sync" : {
            "interval" : {
                "seconds" : 0
                "minutes" : 0
                "hours" : 1
                "days" : 0
                "months" : 0
            },
            "max_batch_size" : 100
        }
    }
    \endcode

    A \c token is used to match the data your application sends to your Qt
    Insight Organization and you can find your token from the
    \l{https://insight.qt.io}{Qt Insight Console}.

    Some of the configuration items are directly linked to the
    \l{https://docs.snowplow.io/docs/collecting-data/collecting-from-own-applications/snowplow-tracker-protocol}{Snowblow Tracker Protocol},
    which defines the allowed values. These include QInsightConfiguration::setPlatform()
    and QInsightConfiguration::setDeviceScreenType().

    QInsightConfiguration::setStorageType() can be used to configure the storage type,
    where \c SQLITE is the default value and use SQLite database to store the events
    before they are sent to the back-end server. If an unknown storage type is specified
    or it is explicitly set to an empty string, caching will be disabled.
    In that case the tracked data will be lost if the back-end server isn't available.

    QInsightConfiguration::setStoragePath() can be used to configure the location
    of the storage. By default, the SQLite database is created in the application's
    directory.

    QInsightConfiguration::setSyncInterval() can be used to configure how often
    the tracked events are sent to the back-end server. The API takes the value in
    seconds, while the json configuration file can be configured also with more natural
    minutes, hours, days, and months. If sync interval is not set, events will never
    be sent to the backend and cached only in the local storage.

    QInsightConfiguration::setBatchSize is used to minimize the overhead when
    sending, reading and writing to the storage. When synchronizing the cached
    data to back-end server, at most QInsightConfiguration::batchSize() items
    will be fetched at once from the storage and sent to back-end server. This is more
    optimal, than fetching the data one by one. The sent data will be removed
    from the storage also in batches. The process repeats, until all data is
    sent. If one batch fails, the synchronization will stop, as the next
    batches will likely also fail.
*/

/*!
    \qmltype InsightConfiguration
    \inqmlmodule QtInsightTracker
    \instantiates QInsightConfiguration
    \brief Provides an interface to the Qt Insight Tracker configuration.
*/

/*!
    Construct a new configuration object with the given \a parent.
 */
QInsightConfiguration::QInsightConfiguration(QObject *parent) : QObject(parent)
{
    static bool loaded = false;
    if (!loaded) {
        load();
        loaded = true;
    }
}

/*!
    Load existing configurations from a file.
    \return true is configuration was successfully loaded.
 */
bool QInsightConfiguration::load()
{
    QString configPath = qEnvironmentVariable("QT_INSIGHT_CONFIG");
    if (configPath.isEmpty()) {
        configPath = QLatin1String(":/qtinsight.conf");
        if (!QFile::exists(configPath))
            configPath = QDir(QCoreApplication::applicationDirPath())
                                 .absoluteFilePath(DEFAULT_CONF_FILE);
        if (!QFile::exists(configPath))
            configPath = DEFAULT_CONF_FILE;
        if (!QFile::exists(configPath))
            configPath =
                    QStandardPaths::locate(QStandardPaths::GenericConfigLocation,
                                           QString::fromLatin1("QtProject/") + DEFAULT_CONF_FILE);
        if (configPath.isEmpty())
            configPath = QDir(QLibraryInfo::path(QLibraryInfo::DataPath))
                                 .absoluteFilePath(QString::fromLatin1("qtinsight/")
                                                   + DEFAULT_CONF_FILE);
        if (!QFile::exists(configPath)) {
            qCWarning(lcQInsightConfig) << "Could not find configuration file:" << DEFAULT_CONF_FILE;
            return false;
        }
    }
    QFile file(configPath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        qCWarning(lcQInsightConfig) << "Could no open configuration file" << configPath;
        return false;
    }
    qCInfo(lcQInsightConfig) << "Using configurations from:" << configPath;

    const auto data = file.readAll();
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        qCWarning(lcQInsightConfig) << "Invalid JSON configuration file: " << configPath;
        return false;
    }

    // reset to default values
    configuration() = Configuration();

    const auto configObj = doc.object();
    setServer(configObj[QLatin1String("server")].toString());
    configuration().m_token = configObj[QLatin1String("token")].toString();

    const auto screenType = configObj[QLatin1String("device_screen_type")].toString();
    setDeviceScreenType(screenType);

    const auto platform = configObj[QLatin1String("platform")].toString();
    setPlatform(platform);

    configuration().m_deviceModel = configObj[QLatin1String("device_model")].toString();
    configuration().m_deviceVariant = configObj[QLatin1String("device_variant")].toString();
    configuration().m_appBuild = configObj[QLatin1String("app_build")].toString();

    configuration().m_storageType =
            configObj.value(QLatin1String("storage")).toString(configuration().m_storageType);
    configuration().m_storagePath = configObj.value(QLatin1String("storage_path")).toString();
    configuration().m_storageSize =
            configObj.value(QLatin1String("storage_size")).toInt(configuration().m_storageSize);

    const QJsonObject syncObject = configObj[QLatin1String("sync")].toObject();
    if (!syncObject.isEmpty()) {
        configuration().m_batchSize = syncObject.value(QLatin1String("max_batch_size"))
                                              .toInt(configuration().m_batchSize);

        const QJsonObject syncInterval = syncObject.value(QLatin1String("interval")).toObject();
        if (!syncInterval.isEmpty())
            configuration().m_syncInterval = intervalToSec(syncInterval);
    }

    const QJsonArray categoryArray = configObj[QLatin1String("categories")].toArray();
    for (const auto &category : categoryArray)
        configuration().m_categories.push_back(category.toString());

    const QJsonArray eventsArray = configObj[QLatin1String("events")].toArray();
    for (const auto &event : eventsArray)
        configuration().m_events.push_back(event.toString());

    return true;
}

/*!
    \return true if configuratio is valid.
 */
bool QInsightConfiguration::isValid()
{
    // server address is mandatory when sync is enabled or caching is disabled
    if ((configuration().m_syncInterval > 0 || configuration().m_storageType.isEmpty())
        && configuration().m_server.isEmpty()) {
        qCWarning(lcQInsightConfig) << "Missing server address";
        return false;
    }

    // token is always mandatory
    if (configuration().m_token.isEmpty()) {
        qCWarning(lcQInsightConfig) << "missing token";
        return false;
    }

    return true;
}

/*!
    \qmlproperty string InsightConfiguration::server
*/

/*!
    \return current server address.
 */
QString QInsightConfiguration::server() const
{
    return configuration().m_server;
}

/*!
    Set server address to \a server.
 */
void QInsightConfiguration::setServer(const QString &server)
{
    if (server.startsWith(QLatin1String("http")))
        qCWarning(lcQInsightConfig) << "Invalid server address" << server;
    else
        configuration().m_server = server;
}

/*!
    \qmlproperty string InsightConfiguration::token
*/

/*!
    \return current server token.
 */
QString QInsightConfiguration::token() const
{
    return configuration().m_token;
}

/*!
    Set server token to \a token.
 */
void QInsightConfiguration::setToken(const QString &token)
{
    configuration().m_token = token;
}

/*!
    \qmlproperty string InsightConfiguration::deviceModel
*/

/*!
    \return current device model.
 */
QString QInsightConfiguration::deviceModel() const
{
    return configuration().m_deviceModel;
}

/*!
    Set device model to \a deviceModel.
 */
void QInsightConfiguration::setDeviceModel(const QString &deviceModel)
{
    configuration().m_deviceModel = deviceModel;
}

/*!
    \qmlproperty string InsightConfiguration::deviceVariant
*/

/*!
    \return current device variant.
 */
QString QInsightConfiguration::deviceVariant() const
{
    return configuration().m_deviceVariant;
}

/*!
    Set device variant to \a deviceVariant.
 */
void QInsightConfiguration::setDeviceVariant(const QString &deviceVariant)
{
    configuration().m_deviceVariant = deviceVariant;
}

/*!
    \qmlproperty string InsightConfiguration::deviceScreenType
*/

/*!
    \return current screen type.
 */
QString QInsightConfiguration::deviceScreenType() const
{
    return configuration().m_deviceScreenType;
}

/*!
    Set device screen type to \a deviceScreenType.
 */
void QInsightConfiguration::setDeviceScreenType(const QString &deviceScreenType)
{
    if (!deviceScreenType.isEmpty()) {
        if (deviceScreenType == QLatin1String("TOUCH")
            || deviceScreenType == QLatin1String("NON_TOUCH"))
            configuration().m_deviceScreenType = deviceScreenType;
        else
            qCWarning(lcQInsightConfig)
                    << "Invalid screen type is specified, possible values are: TOUCH, NON_TOUCH";
    }
}

/*!
    \qmlproperty string InsightConfiguration::appBuild
*/

/*!
    \return current application's build version.
 */
QString QInsightConfiguration::appBuild() const
{
    return configuration().m_appBuild;
}

/*!
    Set application's build version \a appBuild.
 */
void QInsightConfiguration::setAppBuild(const QString &appBuild)
{
    configuration().m_appBuild = appBuild;
}

/*!
    \qmlproperty string InsightConfiguration::platform
*/

/*!
    \return current platform.
 */
QString QInsightConfiguration::platform() const
{
    return configuration().m_platform;
}

/*!
    Set platform to \a platform
 */
void QInsightConfiguration::setPlatform(const QString &platform)
{
    if (!platform.isEmpty()) {
        if (SNOWPLOW_PLATFORMS.contains(platform))
            configuration().m_platform = platform;
        else
            qCWarning(lcQInsightConfig)
                    << "Invalid platform is specified, possible values are:" << SNOWPLOW_PLATFORMS;
    }
}

/*!
    \qmlproperty string InsightConfiguration::storageType
*/

/*!
    \return current storage type.
 */
QString QInsightConfiguration::storageType() const
{
    return configuration().m_storageType;
}

/*!
    \brief Set storage type to \a storageType.
 */
void QInsightConfiguration::setStorageType(const QString &storageType)
{
    configuration().m_storageType = storageType;
}

/*!
    \qmlproperty string InsightConfiguration::storagePath
*/

/*!
    \return current storage path.
 */
QString QInsightConfiguration::storagePath() const
{
    return configuration().m_storagePath;
}

/*!
    Set storage path to \a storagePath.
 */
void QInsightConfiguration::setStoragePath(const QString &storagePath)
{
    configuration().m_storagePath = storagePath;
}

/*!
    \qmlproperty int InsightConfiguration::storageSize
*/

/*!
    \return current storage size.
 */
int QInsightConfiguration::storageSize() const
{
    return configuration().m_storageSize;
}

/*!
    Set storage size to \a storageSize.
 */
void QInsightConfiguration::setStorageSize(int storageSize)
{
    configuration().m_storageSize = storageSize;
}

/*!
    \qmlproperty int InsightConfiguration::syncInterval
*/

/*!
    \return current sync interval in seconds.
 */

int QInsightConfiguration::syncInterval() const
{
    return configuration().m_syncInterval;
}

/*!
    Set sync interval to \a syncInterval seconds.
 */
void QInsightConfiguration::setSyncInterval(int syncInterval)
{
    configuration().m_syncInterval = syncInterval;
}

/*!
    \qmlproperty int InsightConfiguration::batchSize
*/

/*!
    \return current sync batch size.
 */
int QInsightConfiguration::batchSize() const
{
    return configuration().m_batchSize;
}

/*!
    Set sync batch size to \a batchSize.
    Defines how many events are sent to network in one go.
 */
void QInsightConfiguration::setBatchSize(int batchSize)
{
    configuration().m_batchSize = batchSize;
}

/*!
    \qmlproperty stringlist InsightConfiguration::categories
*/

/*!
    \return currently enabled categories.
 */
QStringList QInsightConfiguration::categories()
{
    return configuration().m_categories;
}

/*!
    Set list of enabled categories to \a categories.
 */
void QInsightConfiguration::setCategories(const QStringList &categories)
{
    configuration().m_categories = categories;
}

/*!
    \return currently enabled events.
 */
QStringList QInsightConfiguration::events()
{
    return configuration().m_events;
}

/*!
    Set list of enabled events to \a events.
 */
void QInsightConfiguration::setEvents(const QStringList &events)
{
    configuration().m_categories = events;
}
