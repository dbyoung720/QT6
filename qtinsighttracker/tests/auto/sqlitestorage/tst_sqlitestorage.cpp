// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial

#include <QtTest>

#include <QtInsightTracker/private/qsqlitestorage_p.h>

static const auto TEST_DB_NAME = QLatin1String(":memory:");

class tst_SQLiteStorage : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testAddRemove();
    void testClearCache();
    void testMaxRecords();
    void testOffset();
    void testSize();
    void testBug116699();
};

void tst_SQLiteStorage::testAddRemove()
{
    auto cleanup = [](QSQLiteStorage *s) {
        delete s;
        QFile::remove(TEST_DB_NAME);
    };
    std::unique_ptr<QSQLiteStorage, decltype(cleanup)> storage(new QSQLiteStorage(TEST_DB_NAME),
                                                              cleanup);
    QVERIFY(storage->open());

    QList<QByteArray> testData = { "data1", "data2", "data3", "data4", "data5" };

    // Test insertion
    storage->add(testData[0]);
    QList<QInsightStorage::EventData> data = storage->get(1);
    QCOMPARE(data.size(), 1);
    QCOMPARE(data[0].id, 1ULL);
    QCOMPARE(data[0].payload, testData[0]);

    data = storage->get(10);
    QCOMPARE(data.size(), 1);
    QCOMPARE(data[0].id, 1ULL);
    QCOMPARE(data[0].payload, testData[0]);

    for (int i = 1; i < 5; ++i)
        storage->add(testData[i]);

    data = storage->get(2);
    QCOMPARE(data.size(), 2);
    QCOMPARE(data[0].id, 1ULL);
    QCOMPARE(data[0].payload, testData[0]);
    QCOMPARE(data[1].id, 2ULL);
    QCOMPARE(data[1].payload, testData[1]);

    data = storage->get(testData.size());
    QCOMPARE(data.size(), testData.size());
    for (int i = 0; i < testData.size(); ++i) {
        QCOMPARE(data[i].id, quint64(i + 1));
        QCOMPARE(data[i].payload, testData[i]);
    }

    data = storage->get(testData.size() + 10);
    QCOMPARE(data.size(), testData.size());
    for (int i = 0; i < testData.size(); ++i) {
        QCOMPARE(data[i].id, quint64(i + 1));
        QCOMPARE(data[i].payload, testData[i]);
    }

    // Test deletion
    storage->remove(2);
    data = storage->get(testData.size());

    auto it = std::find_if(data.begin(), data.end(), [](auto event) {
        return event.id == 2;
    });
    QVERIFY(it == data.end());

    testData.removeAt(1); // The item with id=2 is at index 1 in the testData
    QCOMPARE(data.size(), testData.size());
    for (int i = 0; i < testData.size(); ++i)
        QCOMPARE(data[i].payload, testData[i]);

    storage->remove(QSet<quint64>{3, 5});
    data = storage->get(testData.size());
    it = std::find_if(data.begin(), data.end(), [](auto event) {
        return event.id == 3 || event.id == 5;
    });
    QVERIFY(it == data.end());

    testData.removeAt(1); // removes "data3"
    testData.removeAt(2); // removes "data5"
    QCOMPARE(data.size(), testData.size());
    for (int i = 0; i < testData.size(); ++i)
        QCOMPARE(data[i].payload, testData[i]);
}

void tst_SQLiteStorage::testClearCache()
{
    auto cleanup = [](QSQLiteStorage *s) {
        delete s;
        QFile::remove(TEST_DB_NAME);
    };
    std::unique_ptr<QSQLiteStorage, decltype(cleanup)> storage(new QSQLiteStorage(TEST_DB_NAME),
                                                               cleanup);
    QVERIFY(storage->open());

    QList<QByteArray> testData = { "data1", "data2", "data3", "data4", "data5" };

    for (int i = 0; i < 5; ++i)
        storage->add(testData[i]);

    QList<QInsightStorage::EventData> data = storage->get(testData.size() + 10);
    QCOMPARE(data.size(), testData.size());

    QVERIFY(storage->removeAll());

    data = storage->get(testData.size() + 10);
    QCOMPARE(data.size(), 0);
}

void tst_SQLiteStorage::testMaxRecords()
{
    auto cleanup = [](QSQLiteStorage *s) {
        delete s;
        QFile::remove(TEST_DB_NAME);
    };
    std::unique_ptr<QSQLiteStorage, decltype(cleanup)> storage(new QSQLiteStorage(TEST_DB_NAME, 3),
                                                              cleanup);
    QVERIFY(storage->open());

    QList<QByteArray> testData = { "data1", "data2", "data3", "data4", "data5" };

    for (int i = 0; i < 5; ++i)
        storage->add(testData[i]);

    QList<QInsightStorage::EventData> data = storage->get(5);
    QCOMPARE(data.size(), 3);
    QCOMPARE(data[0].id, 3ULL);
    QCOMPARE(data[0].payload, testData[2]);
    QCOMPARE(data[1].id, 4ULL);
    QCOMPARE(data[1].payload, testData[3]);
    QCOMPARE(data[2].id, 5ULL);
    QCOMPARE(data[2].payload, testData[4]);
}

void tst_SQLiteStorage::testOffset()
{
    auto cleanup = [](QSQLiteStorage *s) {
        delete s;
        QFile::remove(TEST_DB_NAME);
    };
    std::unique_ptr<QSQLiteStorage, decltype(cleanup)> storage(new QSQLiteStorage(TEST_DB_NAME),
                                                               cleanup);
    QVERIFY(storage->open());

    QCOMPARE(storage->size(), 0);

    QList<QByteArray> testData = { "data1", "data2", "data3", "data4", "data5" };

    for (const auto &data : testData)
        storage->add(data);

    QList<QInsightStorage::EventData> data = storage->get(2, 2);
    QCOMPARE(data.size(), 2);
    QCOMPARE(data[0].id, 3ULL);
    QCOMPARE(data[0].payload, testData[2]);
    QCOMPARE(data[1].id, 4ULL);
    QCOMPARE(data[1].payload, testData[3]);
}

void tst_SQLiteStorage::testSize()
{
    auto cleanup = [](QSQLiteStorage *s) {
        delete s;
        QFile::remove(TEST_DB_NAME);
    };
    std::unique_ptr<QSQLiteStorage, decltype(cleanup)> storage(new QSQLiteStorage(TEST_DB_NAME),
                                                               cleanup);
    QVERIFY(storage->open());

    QCOMPARE(storage->size(), 0);

    QList<QByteArray> testData = { "data1", "data2", "data3", "data4", "data5" };

    for (const auto &data : testData)
        storage->add(data);

    QCOMPARE(storage->size(), testData.size());
}

void tst_SQLiteStorage::testBug116699()
{
    QTemporaryFile temp;
    QVERIFY(temp.open());

    std::unique_ptr<QSQLiteStorage> storage(new QSQLiteStorage(temp.fileName(), 3));
    QVERIFY(storage->open());
    storage.reset(new QSQLiteStorage(temp.fileName(), 3));
    QVERIFY(storage->open());
}

QTEST_MAIN(tst_SQLiteStorage)

#include "tst_sqlitestorage.moc"
