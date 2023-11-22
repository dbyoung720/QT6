// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial

#include <QtInsightTracker/private/qinsightreporter_p.h>
#include <QtInsightTracker/private/qsqlitestorage_p.h>
#include <QtCore/QCommandLineParser>
#include <QtCore/QCoreApplication>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtNetwork/QNetworkReply>
#include <QtCore/QElapsedTimer>

const int UPLOAD_BATCH = 100;

class QInsightUploader : public QObject
{
    Q_OBJECT
public:
    QInsightUploader(const QString &server, const QStringList &databases);
    void upload();

private:
    void sync();
    void nextDatabase();
    void sendToBackend(const QByteArray &payload);

private:
    std::unique_ptr<QSQLiteStorage> m_storage = nullptr;
    QUrl m_url;
    QNetworkAccessManager *m_qnam = nullptr;
    QStringList m_databases;
    QString n_database;
    quint64 m_events = 0;
    quint64 m_bytes = 0;
    quint64 m_sentEvents = 0;
    quint64 m_sentBytes = 0;
    QElapsedTimer timer;
    double m_speed;
};

QInsightUploader::QInsightUploader(const QString &server, const QStringList &databases)
    : m_qnam(new QNetworkAccessManager(this)), m_databases(databases)

{
    m_url = QUrl(INSIGHT_SERVER_URL.arg(server));

    connect(m_qnam, &QNetworkAccessManager::finished, this, [this] {
        double time = timer.elapsed() / 1000.0;
        m_speed = m_sentBytes / time / 1024;
    });

#ifndef QT_NO_SSL
    // Accept self-signed certificate on localhost for easy testing
    if (server == QLatin1String("localhost"))
        connect(m_qnam, &QNetworkAccessManager::sslErrors, this,
                [](QNetworkReply *reply) { reply->ignoreSslErrors(); });
#endif
}

void QInsightUploader::upload()
{
    nextDatabase();
    sync();
}

void QInsightUploader::nextDatabase()
{
    m_sentEvents = 0;
    if (!m_databases.isEmpty()) {
        n_database = m_databases.takeFirst();
        m_storage = std::make_unique<QSQLiteStorage>(n_database);
        if (!m_storage->open()) {
            fprintf(stderr, "Failed to open the database\n");
            exit(EXIT_FAILURE);
        }
        m_events = m_storage->size();
        QFileInfo info(n_database);
        m_bytes = info.size();
    } else {
        // all done
        fprintf(stdout, "Upload completed.\n");
        exit(EXIT_SUCCESS);
    }
}

void QInsightUploader::sync()
{
    const auto events = m_storage->get(UPLOAD_BATCH, m_sentEvents);
    if (!events.isEmpty()) {
        QJsonObject requestData;
        requestData[SNOWPLOW_SCHEMA] = SNOWPLOW_SCHEMA_PAYLOAD_DATA;
        QJsonArray array;
        for (const auto &event : events)
            array.append(QJsonDocument::fromJson(event.payload).object());
        requestData[SNOWPLOW_DATA] = array;
        const auto payload = QJsonDocument(requestData).toJson(QJsonDocument::Compact);
        fprintf(stdout, "Uploading %s %llu/%llu events (%.2f KiB/s)\r", qPrintable(n_database),
                m_sentEvents, m_events, m_speed);
        m_sentEvents += events.size();
        m_sentBytes = payload.size();
        sendToBackend(payload);
    } else {
        fprintf(stdout, "Uploading %s done, %llu events sent. (%.0f KiB/s)\n",
                qPrintable(n_database), m_sentEvents, m_speed);
        // start again with next database
        QMetaObject::invokeMethod(this, &QInsightUploader::upload);
    }
}

void QInsightUploader::sendToBackend(const QByteArray &payload)
{
    QNetworkRequest request(m_url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QLatin1String("application/json"));

    auto reply = m_qnam->post(request, payload);
    timer.restart();
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        if (reply->error() != QNetworkReply::NoError) {
            fprintf(stderr, "\nUpload failed: %s\n", qPrintable(reply->errorString()));
            exit(EXIT_FAILURE);
        }
        reply->deleteLater();
        sync();
    });
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("insightuploader"));
    QCoreApplication::setApplicationVersion(QLatin1String(QT_VERSION_STR));

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption serverOption(
            QStringLiteral("server"),
            QCoreApplication::translate("main", "Insight backend server address"),
            QCoreApplication::translate("main", "server address"));
    parser.addOption(serverOption);

    QCommandLineOption verboseOption(QStringLiteral("verbose"),
                                     QCoreApplication::translate("main", "Verbose output"));
    parser.addOption(verboseOption);

    parser.addPositionalArgument(QStringLiteral("[database(s)]"),
                                 QStringLiteral("Insight Tracker database file(s) to upload."));

    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
    parser.process(app);

    const QStringList databases = parser.positionalArguments();
    if (databases.isEmpty()) {
        parser.showHelp();
        exit(EXIT_FAILURE);
    }
    for (const auto &database : databases) {
        if (!QFileInfo::exists(database)) {
            fprintf(stderr, "Database not found: %s\n", qPrintable(database));
            exit(EXIT_FAILURE);
        }
    }

    if (!parser.isSet(serverOption)) {
        fprintf(stderr, "Server address missing.\n");
        parser.showHelp();
        exit(EXIT_FAILURE);
    }

    QInsightUploader uploader(parser.value(serverOption), databases);
    uploader.upload();

    return app.exec();
}

#include "insightuploader.moc"
