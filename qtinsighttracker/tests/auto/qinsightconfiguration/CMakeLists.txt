# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: LicenseRef-Qt-Commercial

# Collect test data
file(GLOB_RECURSE test_data_glob
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    testdata/*)
list(APPEND test_data ${test_data_glob})

qt_internal_add_test(tst_qinsightconfiguration
    SOURCES
        tst_qinsightconfiguration.cpp
    LIBRARIES
        Qt::InsightTracker
    TESTDATA ${test_data}
)
