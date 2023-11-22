// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial

#include <QtTest>

#include <QtInsightTracker/private/qsqlitestorage_p.h>

static const auto TEST_DB_NAME = QLatin1String("database");

class tst_storage : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testWriteSpeed_data();
    void testWriteSpeed();
    void testReadSpeed_data();
    void testReadSpeed();
    void testRemoveSpeed_data();
    void testRemoveSpeed();
};

void tst_storage::testWriteSpeed_data()
{
    QTest::addColumn<int>("maxRecords");
    QTest::newRow("unlimited records") << 0;
    QTest::newRow("max 1 record") << 1;
    QTest::newRow("max 100 records") << 100;
}

void tst_storage::testWriteSpeed()
{
    QFETCH(int, maxRecords);

    auto cleanup = [](QSQLiteStorage *s) {
        delete s;
        QFile::remove(TEST_DB_NAME);
    };
    std::unique_ptr<QSQLiteStorage, decltype(cleanup)> storage(new QSQLiteStorage(TEST_DB_NAME, maxRecords),
                                                               cleanup);
    QVERIFY(storage->open());

    QByteArray data(700, '@');
    for (int i = 0; i < maxRecords; i++) {
        storage->add(data);
    }

    QBENCHMARK(storage->add(data));
}

void tst_storage::testReadSpeed_data()
{
    QTest::addColumn<int>("batchSize");
    QTest::newRow("1 record") << 1;
    QTest::newRow("10 records") << 10;
    QTest::newRow("100 records") << 100;
}

void tst_storage::testReadSpeed()
{
    QFETCH(int, batchSize);

    auto cleanup = [](QSQLiteStorage *s) {
        delete s;
        QFile::remove(TEST_DB_NAME);
    };
    std::unique_ptr<QSQLiteStorage, decltype(cleanup)> storage(new QSQLiteStorage(TEST_DB_NAME),
                                                               cleanup);
    QVERIFY(storage->open());

    QByteArray data(700, '@');
    for (int i = 0; i < batchSize; i++) {
        storage->add(data);
    }

    QBENCHMARK(storage->get(batchSize));
}

void tst_storage::testRemoveSpeed_data()
{
    QTest::addColumn<int>("batchSize");
    QTest::newRow("1 record") << 1;
    QTest::newRow("10 records") << 10;
    QTest::newRow("100 records") << 100;
}

void tst_storage::testRemoveSpeed()
{
    QFETCH(int, batchSize);

    auto cleanup = [](QSQLiteStorage *s) {
        delete s;
        QFile::remove(TEST_DB_NAME);
    };
    std::unique_ptr<QSQLiteStorage, decltype(cleanup)> storage(new QSQLiteStorage(TEST_DB_NAME),
                                                              cleanup);
    QVERIFY(storage->open());

    QByteArray data(700, '@');
    QSet<quint64> ids;
    for (int i = 0; i < batchSize; i++) {
        storage->add(data);
        ids.insert(i);
    }

    QBENCHMARK(storage->remove(ids));
}

QTEST_MAIN(tst_storage)

#include "tst_storage.moc"
