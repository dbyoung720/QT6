# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: LicenseRef-Qt-Commercial

qt_internal_add_test(tst_insightreporter
    SOURCES
        tst_insightreporter.cpp
    LIBRARIES
        Qt::InsightTrackerPrivate
)
