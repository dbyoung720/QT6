// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial

#include <QtInsightTracker/private/qinsightreporter_p.h>
#include <QtInsightTracker/private/qsqlitestorage_p.h>
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QLibraryInfo>
#include <QtCore/QSettings>
#include <QtCore/QStandardPaths>
#include <QtCore/QSysInfo>
#include <QtCore/QThread>
#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>
#include <QtNetwork/QNetworkReply>

Q_LOGGING_CATEGORY(lcInsightStorage, "qt.insight.storage", QtWarningMsg)
Q_LOGGING_CATEGORY(lcInsightEvents, "qt.insight.events", QtWarningMsg)

namespace {

QString getScreenResolution()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) {
        qCWarning(lcInsightEvents) << "Failed to get the screen resolution.";
        return QString();
    }

    const auto size = screen->size();
    return QStringLiteral("%1x%2").arg(size.width()).arg(size.height());
}

QString getUserIdentifier()
{
    QSettings globalSettings(QStringLiteral("QtProject"),
                             QStringLiteral("InsightTrackerGlobal"));
    const auto v = globalSettings.value(SETTINGS_USER_ID);
    if (v.isNull()) {
        QString uuid = QUuid::createUuid().toString(QUuid::StringFormat::WithoutBraces);
        globalSettings.setValue(SETTINGS_USER_ID, uuid);
        qCDebug(lcInsightEvents) << "No user identifier was found, generated a new one:" << uuid;
        return uuid;
    }
    const QString uuid = v.toString();
    Q_ASSERT(!uuid.isEmpty());
    qCDebug(lcInsightEvents) << "The user identifier:" << uuid;
    return uuid;
}

} // unnamed namespace

QInsightReporter::QInsightReporter()
    : m_appName(QCoreApplication::applicationName()),
      m_userId(getUserIdentifier()),
      m_qnam(new QNetworkAccessManager(this)),
      m_syncTimer(this)
{
    startNewSession();
}

QInsightReporter::~QInsightReporter()
{
}

void QInsightReporter::init(QInsightConfiguration *config)
{
    Q_ASSERT(config);
    m_config = config;
    m_settings = new QSettings(
            QStringLiteral("QtProject"),
            QStringLiteral("InsightTracker%1")
                    .arg(m_appName.isEmpty() ? QString() : QLatin1String("_") + m_appName),
            this);

    m_url = QUrl(INSIGHT_SERVER_URL.arg(m_config->server()));

    if (m_config->storageType().isEmpty()) {
        qCWarning(lcInsightStorage) << "An empty storage type is specified, caching will be disabled.";
    } else if (m_config->storageType() == QLatin1String("SQLITE")) {
#ifdef QT_SQL_LIB

        const auto storagePath = !m_config->storagePath().isEmpty()
                ? m_config->storagePath()
                : QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(storagePath);
        const auto storage = QLatin1String("%1/InsightTracker.db").arg(storagePath);
        qCDebug(lcInsightStorage) << "Using event storage:" << storage;
        m_storage = std::make_unique<QSQLiteStorage>(storage, m_config->storageSize());
#else
        qCWarning(lcInsightStorage) << "QtSql is not configured, caching will be disabled.";
#endif
    } else {
        qCWarning(lcInsightStorage) << m_config->storageType() << "isn't a valid storage type,"
                   << "caching will be disabled.";
    }

    if (m_storage && !m_storage->open()) {
        qCWarning(lcInsightStorage) << "Failed to open the SQLITE storage";
        m_storage = nullptr;
    }

    if (m_storage && m_config->syncInterval() > 0) {
        m_syncTimer.setInterval(qMax(m_config->syncInterval() * 1000, MINIMUM_SYNC_INTERVAL));
        m_syncTimer.setSingleShot(true);

        connect(&m_syncTimer, &QTimer::timeout, this, &QInsightReporter::sync);
        connect(QThread::currentThread(), &QThread::finished, &m_syncTimer, &QTimer::stop);

        const auto v = m_settings->value(SETTINGS_LAST_SYNC_TIME);
        if (!v.isNull()) {
            const auto lastSyncTime = v.toDateTime();
            const int timeSinceLastSync = lastSyncTime.secsTo(QDateTime::currentDateTime());
            qCDebug(lcInsightEvents) << "Last sync at" << lastSyncTime
                                       << timeSinceLastSync <<"seconds ago.";
            /* pretend timer has been running the whole time */
            m_syncTimer.setInterval(qMax((m_config->syncInterval() - timeSinceLastSync) * 1000,
                                         MINIMUM_SYNC_INTERVAL));
        } else {
            /* mark the first application launch also as the last sync */
            m_settings->setValue(SETTINGS_LAST_SYNC_TIME, QDateTime::currentDateTime());
        }
        m_syncTimer.start();
        qCInfo(lcInsightEvents) << "Next sync in" << m_syncTimer.remainingTime() / 1000 << "seconds";
    }

    if (!m_storage || m_syncTimer.isActive())
        qCInfo(lcInsightEvents) << "Sending events to" << m_url.toDisplayString();
    else
        qCInfo(lcInsightEvents())
                << "Working in offline mode, events are cached only to local storage";

    trackDeviceAndAppInfo();

    auto shutdown = [this] {
        m_settings->setValue(SETTINGS_LAST_SHUTDOWN, true);
    };
    connect(QThread::currentThread(), &QThread::finished, this, shutdown);
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, shutdown);
    m_settings->setValue(SETTINGS_LAST_SHUTDOWN, false);
}

void QInsightReporter::trackDeviceAndAppInfo()
{
    Q_ASSERT(m_config);

    QJsonObject deviceInfoContext;
    deviceInfoContext[SNOWPLOW_SCHEMA] = SNOWPLOW_SCHEMA_DEVICE;
    QJsonObject deviceData;
    if (!m_config->deviceModel().isEmpty())
        deviceData[QLatin1String("model")] = m_config->deviceModel();
    if (!m_config->deviceVariant().isEmpty())
        deviceData[QLatin1String("variant")] = m_config->deviceVariant();
    if (!m_config->deviceScreenType().isEmpty())
        deviceData[QLatin1String("screen_type")] = m_config->deviceScreenType();
    deviceData[QLatin1String("screen_resolution")] = getScreenResolution();
    deviceData[QLatin1String("cpu_architecture")] = QSysInfo::currentCpuArchitecture();
    deviceData[QLatin1String("kernel_type")] = QSysInfo::kernelType();
    deviceData[QLatin1String("kernel_version")] = QSysInfo::kernelVersion();
    deviceData[QLatin1String("os_type")] = QSysInfo::productType();
    deviceData[QLatin1String("os_version")] = QSysInfo::productVersion();

    deviceInfoContext[SNOWPLOW_DATA] = deviceData;

    QJsonObject appInfoContext;
    appInfoContext[SNOWPLOW_SCHEMA] = SNOWPLOW_SCHEMA_APPLICATION;
    QJsonObject appData;
    if (!m_config->appBuild().isEmpty())
        appData[QLatin1String("build")] = m_config->appBuild();
    appData[QLatin1String("version")] = QCoreApplication::instance()->applicationVersion();
    appData[QLatin1String("qt_version")] = QLatin1String(qVersion());
    appData[QLatin1String("last_shutdown")] = m_settings->value(SETTINGS_LAST_SHUTDOWN).toBool();
    appInfoContext[SNOWPLOW_DATA] = appData;

    QJsonArray contextArray;
    contextArray.append(deviceInfoContext);
    contextArray.append(appInfoContext);

    addEvent(contextArray);
}

void QInsightReporter::trackScreenView(const QString &name,
                                       const std::optional<InsightTracker::ContextData> &context)
{
    QJsonObject screenViewContext;
    screenViewContext[SNOWPLOW_SCHEMA] = SNOWPLOW_SCHEMA_SCREEN_VIEW;

    QJsonObject screenViewData;
    screenViewData[QLatin1String("object_id")] = name;
    screenViewData[QLatin1String("previous_id")] = m_previousScreen;
    screenViewContext[SNOWPLOW_DATA] = screenViewData;

    m_previousScreen = name;

    QJsonArray contextArray;
    contextArray.append(screenViewContext);

    if (context) {
        QJsonObject contextDataContext;
        contextDataContext[SNOWPLOW_SCHEMA] = SNOWPLOW_SCHEMA_CONTEXT_DATA;

        QJsonObject contextData;
        contextData[QLatin1String("key")] = context->key;
        contextData[QLatin1String("val")] = context->value;
        contextDataContext[SNOWPLOW_DATA] = contextData;

        contextArray.append(contextDataContext);
    }

    addEvent(contextArray);
}

void QInsightReporter::trackClickEvent(const QString &name, const QString &category, int x, int y,
                                       const std::optional<InsightTracker::ContextData> &context)
{
    QJsonObject clickEventContext;
    clickEventContext[SNOWPLOW_SCHEMA] = SNOWPLOW_SCHEMA_EVENT_CLICK;

    QJsonObject clickEventData;
    clickEventData[QLatin1String("object_id")] = name;
    clickEventData[QLatin1String("category")] = category;
    clickEventData[QLatin1String("screen_id")] = m_previousScreen;

    QJsonObject position;
    position[QLatin1String("x")] = x;
    position[QLatin1String("y")] = y;
    clickEventData[QLatin1String("position")] = position;
    clickEventContext[SNOWPLOW_DATA] = clickEventData;

    QJsonArray contextArray;
    contextArray.append(clickEventContext);

    if (context) {
        QJsonObject contextDataContext;
        contextDataContext[SNOWPLOW_SCHEMA] = SNOWPLOW_SCHEMA_CONTEXT_DATA;

        QJsonObject contextData;
        contextData[QLatin1String("key")] = context->key;
        contextData[QLatin1String("val")] = context->value;
        contextDataContext[SNOWPLOW_DATA] = contextData;

        contextArray.append(contextDataContext);
    }

    addEvent(contextArray);
}

void QInsightReporter::trackEvent(
        const QString &event, const QString &target,
        const std::optional<InsightTracker::EventCoordinates> &coordinates,
        const std::optional<QString> &data)
{
    QJsonObject eventContext;
    eventContext[SNOWPLOW_SCHEMA] = SNOWPLOW_SCHEMA_QEVENT;

    QJsonObject eventData;
    eventData[QLatin1String("type")] = event;
    eventData[QLatin1String("object")] = target;
    eventData[QLatin1String("screen_id")] = m_previousScreen;
    if (data)
        eventData[QLatin1String("data")] = data.value();

    if (coordinates) {
        QJsonObject position;
        position[QLatin1String("x")] = coordinates->x;
        position[QLatin1String("y")] = coordinates->y;
        eventData[QLatin1String("position")] = position;
    }
    eventContext[SNOWPLOW_DATA] = eventData;

    QJsonArray contextArray;
    contextArray.append(eventContext);

    addEvent(contextArray);
}

void QInsightReporter::clearCache()
{
    m_storage->removeAll();
}

void QInsightReporter::startNewSession()
{
    m_sessionId = QUuid::createUuid().toString(QUuid::StringFormat::WithoutBraces);
    qCInfo(lcInsightEvents) << "Using new session id:" << m_sessionId;
}

QByteArray QInsightReporter::createUnstructuredEvent(const QJsonArray &contextArray)
{
    QJsonObject unstructuredEvent;
    unstructuredEvent[SNOWPLOW_EVENT] = SNOWPLOW_UNSTRUCTURED_EVENT;
    unstructuredEvent[SNOWPLOW_TRACKER_NAMESPACE] = m_config->token();
    const auto timestamp = QString::number(QDateTime::currentMSecsSinceEpoch());
    unstructuredEvent[SNOWPLOW_EVENT_TIMESTAMP] = timestamp;
    // TODO: We're skipping the optional sent timestamp for now, consider fixing this later
    // unstructuredEvent[SNOWPLOW_EVENT_SENT_TIMESTAMP] = timestamp;
    QString eid = QUuid::createUuid().toString(QUuid::StringFormat::WithoutBraces);
    unstructuredEvent[SNOWPLOW_EVENT_ID] = eid;
    unstructuredEvent[SNOWPLOW_TRACKER_VERSION] = QT_TRACKER_VERSION;
    unstructuredEvent[SNOWPLOW_LANGUAGE] = QLocale::system().name();
    unstructuredEvent[SNOWPLOW_APP_ID] = m_appName;
    unstructuredEvent[SNOWPLOW_SESSION_ID] = m_sessionId;
    unstructuredEvent[SNOWPLOW_USER_ID] = m_userId;

    Q_ASSERT(m_config);
    unstructuredEvent[SNOWPLOW_PLATFORM] = m_config->platform();

    // Add context
    QJsonObject unstructuredEventContext;
    unstructuredEventContext[SNOWPLOW_SCHEMA] = SNOWPLOW_SCHEMA_CONTEXT;
    unstructuredEventContext[SNOWPLOW_DATA] = contextArray;

    const auto coStr = QJsonDocument(unstructuredEventContext).toJson(QJsonDocument::Compact);
    unstructuredEvent[SNOWPLOW_CONTEXT] = QString::fromUtf8(coStr);

    return QJsonDocument(unstructuredEvent).toJson(QJsonDocument::Compact);
}

void QInsightReporter::addEvent(const QJsonArray &contextArray)
{
    const QByteArray payload = createUnstructuredEvent(contextArray);
    qCDebug(lcInsightEvents).noquote().nospace() << "New event (" << payload.size() << " bytes) "
                                                 << payload;

    if (m_storage) {
        m_storage->add(payload);
    } else {
        qCDebug(lcInsightEvents()) << "Storage is not available, sending event immediately.";
        sendToBackend(QSet<quint64>{0}, payload);
    }
}

void QInsightReporter::sync()
{
    Q_ASSERT(m_storage);

    const auto events = m_storage->get(m_config->batchSize());
    if (events.isEmpty()) {
        // No more data to sync, save the last sync time
        const auto currentTime = QDateTime::currentDateTime();
        qCInfo(lcInsightEvents) << "All events sent at" << currentTime.toString();
        m_settings->setValue(SETTINGS_LAST_SYNC_TIME, currentTime);
        m_syncTimer.start(qMax(m_config->syncInterval() * 1000, MINIMUM_SYNC_INTERVAL));
    } else {
        QJsonObject requestData;
        requestData[SNOWPLOW_SCHEMA] = SNOWPLOW_SCHEMA_PAYLOAD_DATA;
        QJsonArray array;
        QSet<quint64> ids;
        for (const auto &event : events) {
            array.append(QJsonDocument::fromJson(event.payload).object());
            ids.insert(event.id);
        }
        requestData[SNOWPLOW_DATA] = array;
        const auto payload = QJsonDocument(requestData).toJson(QJsonDocument::Compact);
        sendToBackend(ids, payload);
    }
}

void QInsightReporter::sendToBackend(const QSet<quint64> &ids, const QByteArray &payload)
{
    qCDebug(lcInsightEvents) << "Sending" << ids.size() << "events in" << payload.size() << "bytes";

    QNetworkRequest request(m_url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QLatin1String("application/json"));

    auto reply = m_qnam->post(request, payload);
    connect(reply, &QNetworkReply::finished, this, [this, ids, reply] {
        int sync = 0;
        if (reply->error() == QNetworkReply::NoError) {
            qCDebug(lcInsightEvents) << "Events have been sent successfully.";
            if (m_storage) {
                qCDebug(lcInsightEvents) << "Remove sent events from storage.";
                if (!m_storage->remove(ids)) {
                    qCCritical(lcInsightEvents) << "Failed to remove events from storage, disable event caching";
                    // If we can't remove the events, we would repeatedly send the same events
                    m_storage = nullptr;
                    m_syncTimer.stop();
                }
            }
        } else {
            qCWarning(lcInsightEvents) << "Failed to sent events:" << reply->errorString();
            // try again at next sync interval
            sync = qMax(m_config->syncInterval() * 1000, MINIMUM_SYNC_INTERVAL);
        }
        reply->deleteLater();
        if (m_storage)
            m_syncTimer.start(sync);
    });
}
