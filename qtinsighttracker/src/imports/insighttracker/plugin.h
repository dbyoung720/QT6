// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial

#ifndef QTINSIGHTTRACKER_PLUGIN_H
#define QTINSIGHTTRACKER_PLUGIN_H

#include <QQmlExtensionPlugin>

class QtInsightTrackerPlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)

public:
    void registerTypes(const char *uri) override;
};

#endif // QTINSIGHTTRACKER_PLUGIN_H
