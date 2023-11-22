// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial

#include <QtInsightTracker/QInsightTracker>
#include <QtGui/QGenericPlugin>
#include <QtCore/QCoreApplication>

QT_BEGIN_NAMESPACE

class QInsightTrackerPlugin : public QGenericPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QGenericPluginFactoryInterface_iid FILE "insighttracker.json")

public:
    QInsightTrackerPlugin();

    QObject* create(const QString &key, const QString &specification) override;

private:
    QInsightTracker *tracker;
};

QInsightTrackerPlugin::QInsightTrackerPlugin()
{
    tracker = new QInsightTracker();
    tracker->setEnabled(true);
}

QObject* QInsightTrackerPlugin::create(const QString &key,
                                         const QString &spec)
{
    Q_UNUSED(spec);
    if (!key.compare(QLatin1String("InsightTracker"), Qt::CaseInsensitive))
        return new QInsightTrackerPlugin();

    return nullptr;
}

QT_END_NAMESPACE

#include "main.moc"
