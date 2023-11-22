// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial

#include "plugin.h"

#include <QtInsightTrackerQml/private/qinsighttrackerqml_p.h>
#include <QtInsightTrackerQml/private/qinsightcategory_p.h>

#include <qqml.h>

void QtInsightTrackerPlugin::registerTypes(const char *uri)
{
    qmlRegisterTypesAndRevisions<QInsightCategory>("QtInsightTracker", 1);
    qmlRegisterTypesAndRevisions<QInsightCategoryAttached>("QtInsightTracker", 1);
    qmlRegisterTypesAndRevisions<QInsightConfigurationForeign>("QtInsightTracker", 1);
    qmlRegisterTypesAndRevisions<QInsightTrackerQml>("QtInsightTracker", 1);
    qmlRegisterModule(uri, 1, 0);
}
