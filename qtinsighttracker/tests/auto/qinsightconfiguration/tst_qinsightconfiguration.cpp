// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial

#include <QtTest>

#include <QtInsightTracker/QInsightConfiguration>

class tst_QInsightConfiguration : public QObject
{
    Q_OBJECT

private slots:
    void defaultValues();
    void loadConfig_data();
    void loadConfig();
    void syncInterval_data();
    void syncInterval();
};

void tst_QInsightConfiguration::defaultValues()
{
    qputenv("QT_INSIGHT_CONFIG", "invalid");

    QInsightConfiguration config;
    QCOMPARE(config.isValid(), false);
    QVERIFY(config.server().isEmpty());
    QVERIFY(config.token().isEmpty());
    QVERIFY(config.deviceModel().isEmpty());
    QVERIFY(config.deviceVariant().isEmpty());
    QVERIFY(config.deviceScreenType().isEmpty());
    QVERIFY(config.appBuild().isEmpty());
    QCOMPARE(config.storageType(), "SQLITE");
    QCOMPARE(config.batchSize(), 100);
    QCOMPARE(config.syncInterval(), 0);
}

void tst_QInsightConfiguration::loadConfig_data()
{
    QTest::addColumn<QString>("path");
    QTest::addColumn<bool>("loaded");
    QTest::addColumn<bool>("valid");
    QTest::addColumn<QString>("server");
    QTest::addColumn<int>("syncInterval");
    QTest::addColumn<int>("batchSize");
    QTest::newRow("wrong_path") << "wrong_path.json"
                                << false
                                << false
                                << ""
                                << 0
                                << 100;
    QTest::newRow("wrong_struct_config") << QFINDTESTDATA("testdata/wrong_struct_config.json")
                                         << false
                                         << false
                                         << ""
                                         << 0
                                         << 100;
    QTest::newRow("invalid_config") << QFINDTESTDATA("testdata/invalid.json")
                                    << false
                                    << false
                                    << ""
                                    << 0
                                    << 100;
    QTest::newRow("missing_server_config") << QFINDTESTDATA("testdata/missing_server.json")
                                           << true
                                           << false
                                           << ""
                                           << 0
                                           << 100;
    QTest::newRow("missing_token_config") << QFINDTESTDATA("testdata/missing_token.json")
                                           << true
                                           << false
                                           << "1.2.3.4:2000"
                                           << 0
                                           << 100;
    QTest::newRow("offline") << QFINDTESTDATA("testdata/offline.json")
                             << true
                             << true
                             << ""
                             << 0
                             << 100;
    QTest::newRow("valid_config") << QFINDTESTDATA("testdata/valid_config.json")
                                  << true
                                  << true
                                  << "1.2.3.4:2000"
                                  << 42
                                  << 43;
}

void tst_QInsightConfiguration::loadConfig()
{
    QFETCH(QString, path);
    QFETCH(bool, loaded);
    QFETCH(bool, valid);
    QFETCH(QString, server);
    QFETCH(int, syncInterval);
    QFETCH(int, batchSize);

    QInsightConfiguration config;
    qputenv("QT_INSIGHT_CONFIG", path.toLatin1());
    auto ok = config.load();
    QCOMPARE(loaded, ok);
    QCOMPARE(config.isValid(), valid);
    if (config.isValid()) {
        QCOMPARE(config.server(), server);
        QCOMPARE(config.token(), "000-000");
        QCOMPARE(config.deviceModel(), "test");
        QCOMPARE(config.deviceVariant(), "v1");
        QCOMPARE(config.deviceScreenType(), "TOUCH");
        QCOMPARE(config.appBuild(), "1.2.3");
        QCOMPARE(config.platform(), "app");
        QCOMPARE(config.storageType(), "SQLITE");
        QCOMPARE(config.syncInterval(), syncInterval);
        QCOMPARE(config.batchSize(), batchSize);
    }
}

qint64 computeInterval(int seconds, int minutes, int hours, int days, int months)
{
    return qint64(seconds + 60 * (minutes + 60 * (hours + 24 * (days + 30 * months))));
}

void tst_QInsightConfiguration::syncInterval_data()
{
    QTest::addColumn<QString>("path");
    QTest::addColumn<qint64>("interval");
    QTest::newRow("custom") << QFINDTESTDATA("testdata/sync_interval.json")
                            << computeInterval(1, 2, 3, 4, 5);
    QTest::newRow("no_hours") << QFINDTESTDATA("testdata/sync_interval_no_hours.json")
                              << computeInterval(1, 2, 0, 4, 5);
    QTest::newRow("no_months") << QFINDTESTDATA("testdata/sync_interval_no_months.json")
                               << computeInterval(1, 2, 3, 4, 0);
}

void tst_QInsightConfiguration::syncInterval()
{
    QFETCH(QString, path);
    QFETCH(qint64, interval);

    QInsightConfiguration config;
    qputenv("QT_INSIGHT_CONFIG", path.toLatin1());
    QVERIFY(config.load());
    QCOMPARE(config.isValid(), true);
    QCOMPARE(config.syncInterval(), interval);
}

QTEST_MAIN(tst_QInsightConfiguration)

#include "tst_qinsightconfiguration.moc"
