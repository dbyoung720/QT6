# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: LicenseRef-Qt-Commercial

qt_internal_add_test(tst_sqlitestorage
    SOURCES
        tst_sqlitestorage.cpp
    LIBRARIES
        Qt::InsightTrackerPrivate
)
