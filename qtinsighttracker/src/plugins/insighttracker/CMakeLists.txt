# Copyright (C) 2023 The Qt Company Ltd.
# SPDX-License-Identifier: LicenseRef-Qt-Commercial

qt_internal_add_plugin(QInsightTrackerPlugin
    OUTPUT_NAME qinsighttracker
    PLUGIN_TYPE generic
    DEFAULT_IF FALSE
    SOURCES
        main.cpp
    LIBRARIES
        Qt::Core
        Qt::InsightTracker
)
