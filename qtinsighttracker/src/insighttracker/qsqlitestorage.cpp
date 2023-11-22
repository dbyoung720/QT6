// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial

#include <QtInsightTracker/private/qsqlitestorage_p.h>

#ifdef QT_SQL_LIB

#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

// Storage constants
static const QLatin1String EVENTS_TABLE_NAME("events");
static const QLatin1String EVENTS_TRIGGER_NAME("events_cleanup");
static const QLatin1String EVENTS_TABLE_ID("id");
static const QLatin1String EVENTS_TABLE_DATA("data");
static const QLatin1String DATBASE_CONNECTION("storage");

namespace {

QString idsToString(const QSet<quint64> &ids)
{
    QStringList idsToStr;
    for (auto id : ids)
        idsToStr.push_back(QString::number(id));
    return idsToStr.join(QLatin1Char(','));
}

} // unnamed namespace

QSQLiteStorage::QSQLiteStorage(const QString &name, int maxRecords)
    : m_name(name), m_maxRecords(maxRecords)
{
}

QSQLiteStorage::~QSQLiteStorage()
{
    if (m_db.isOpen())
        m_db.close();
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(DATBASE_CONNECTION);
}

bool QSQLiteStorage::open()
{
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), DATBASE_CONNECTION);
    m_db.setDatabaseName(m_name);
    if (!m_db.isValid()) {
        qCWarning(lcInsightStorage) <<"Failed to open the storage,"
                                      << m_name <<"isn't a valid database.";
        return false;
    }

    m_db.open();
    if (!m_db.isOpen()) {
        qCWarning(lcInsightStorage) << "Failed to open the storage at"<< m_name;
        return false;
    }

    QSqlQuery query(m_db);
    const auto createTableQuery =
            QLatin1String("CREATE TABLE IF NOT EXISTS %1(%2 INTEGER PRIMARY KEY, %3 STRING)")
                    .arg(EVENTS_TABLE_NAME, EVENTS_TABLE_ID, EVENTS_TABLE_DATA);
    if (!query.exec(createTableQuery)) {
        qCWarning(lcInsightStorage)
                << "Failed to create events table with error:" << query.lastError().text();
        return false;
    }

    const auto dropTriggerQuery = QLatin1String("DROP TRIGGER IF EXISTS %1").arg(EVENTS_TRIGGER_NAME);
    if (!query.exec(dropTriggerQuery)) {
        qCWarning(lcInsightStorage)
                << "Failed to drop events table cleanup trigger with error:"
                << query.lastError().text();
        return false;
    }

    if (m_maxRecords > 0) {
        const auto createTriggerQuery =
                QLatin1String("CREATE TRIGGER %1 AFTER INSERT ON %2 "
                              "BEGIN "
                              "DELETE FROM %3 WHERE id NOT IN "
                              "(SELECT %4 FROM %5 ORDER BY %6 DESC LIMIT %7 ); "
                              "END")
                        .arg(EVENTS_TRIGGER_NAME, EVENTS_TABLE_NAME, EVENTS_TABLE_NAME,
                             EVENTS_TABLE_ID, EVENTS_TABLE_NAME, EVENTS_TABLE_ID)
                        .arg(m_maxRecords);
        if (!query.exec(createTriggerQuery)) {
            qCWarning(lcInsightStorage)
                    << "Failed to create events table cleanup trigger with error:"
                    << query.lastError().text();
            return false;
        }
    }

    return true;
}

bool QSQLiteStorage::add(const QByteArray &payload)
{
    if (!m_db.isOpen())
        return false;

    QSqlQuery query(m_db);

    const QString insertQuery = QLatin1String("INSERT INTO %1(%2) values(?)")
            .arg(EVENTS_TABLE_NAME, EVENTS_TABLE_DATA);

    query.prepare(insertQuery);
    query.addBindValue(payload);
    if (!query.exec()) {
        qCWarning(lcInsightStorage)
                << "Failed to write to storage with error:" << query.lastError().text();
        return false;
    }
    qCDebug(lcInsightStorage).noquote() << "Added a new event";
    return true;
}

bool QSQLiteStorage::remove(quint64 id)
{
    if (!m_db.isOpen())
        return false;

    QSqlQuery query(m_db);
    const QString deleteQuery = QLatin1String("DELETE FROM %1 WHERE %2 = %3;")
            .arg(EVENTS_TABLE_NAME, EVENTS_TABLE_ID, QString::number(id));
    if (!query.exec(deleteQuery)) {
        qCWarning(lcInsightStorage)
                << "Failed to remove the event" << id << "with error:" << query.lastError().text();
        return false;
    }
    qCDebug(lcInsightStorage) << "Removed event" << id;
    return true;
}

bool QSQLiteStorage::remove(const QSet<quint64> &ids)
{
    if (!m_db.isOpen())
        return false;

    QSqlQuery query(m_db);

    const QString deleteQuery = QLatin1String("DELETE FROM %1 WHERE %2 in (%3);")
            .arg(EVENTS_TABLE_NAME, EVENTS_TABLE_ID, idsToString(ids));

    if (!query.exec(deleteQuery)) {
        qCWarning(lcInsightStorage)
                << "Failed to remove events" << ids << "with error:" << query.lastError().text();
        return false;
    }
    qCDebug(lcInsightStorage) << "Removed events" << ids;
    return true;
}

bool QSQLiteStorage::removeAll()
{
    if (!m_db.isOpen())
        return false;

    QSqlQuery query(m_db);
    const QString deleteQuery = QLatin1String("DELETE FROM %1;").arg(EVENTS_TABLE_NAME);

    if (!query.exec(deleteQuery)) {
        qCWarning(lcInsightStorage)
                << "Failed to remove events with error:" << query.lastError().text();
        return false;
    }
    qCDebug(lcInsightStorage) << "Removed all events";
    return true;
}

QList<QSQLiteStorage::EventData> QSQLiteStorage::get(qsizetype n, qsizetype offset)
{
    QList<QSQLiteStorage::EventData> events;

    QSqlQuery query(m_db);
    const QString selectQuery =
            QLatin1String("SELECT * FROM %1 ORDER BY %2 ASC LIMIT %3 OFFSET %4;")
                    .arg(EVENTS_TABLE_NAME, EVENTS_TABLE_ID, QString::number(n),
                         QString::number(offset));

    if (!query.exec(selectQuery)) {
        qCWarning(lcInsightStorage)
                << "Failed to read from storage with error:" << query.lastError().text();
        return events;
    }

    while (query.next()) {
        const QVariant id = query.value(0);
        const QVariant payload = query.value(1);

        Q_ASSERT(id.canConvert<quint64>());
        Q_ASSERT(payload.canConvert<QByteArray>());

        events.append(QSQLiteStorage::EventData(id.toULongLong(), payload.toByteArray()));
    }

    return events;
}

quint64 QSQLiteStorage::size() {
    QSqlQuery query(m_db);
    const QString selectQuery =
            QLatin1String("SELECT COUNT(*) FROM %1;").arg(EVENTS_TABLE_NAME);

    if (!query.exec(selectQuery) || !query.next()) {
        qCWarning(lcInsightStorage)
                << "Failed to read from storage with error:" << query.lastError().text();
        return 0;
    }
    const QVariant count = query.value(0);
    Q_ASSERT(count.canConvert<quint64>());

    return count.toULongLong();
}

#endif // QT_SQL_LIB
