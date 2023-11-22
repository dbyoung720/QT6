// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial

#include <QtTest>

#include <QtInsightTracker/private/qinsightreporter_p.h>

class tst_InsightReporter : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testClickEvent_data();
    void testClickEvent();
};

void tst_InsightReporter::testClickEvent_data()
{
    QTest::addColumn<QString>("storage");
    QTest::addColumn<QString>("path");
    QTest::newRow("sqlite - file") << QString("SQLITE") << "insightdatabase";
    QTest::newRow("sqlite - memory") << QString("SQLITE") << ":memory:";
    QTest::newRow("no storage")<< QString() << QString();
}

void tst_InsightReporter::testClickEvent()
{
    QFETCH(QString, storage);
    QFETCH(QString, path);

    QInsightConfiguration *config = new QInsightConfiguration();
    config->setServer(QLatin1String("localhost"));
    config->setSyncInterval(100000);
    config->setStorageType(storage);
    config->setStoragePath(path);

    QInsightReporter reporter;
    reporter.init(config);

    QBENCHMARK(reporter.trackClickEvent(QLatin1String("button1"), QLatin1String("test"), 0, 0));
}

QTEST_MAIN(tst_InsightReporter)

#include "tst_insightreporter.moc"
