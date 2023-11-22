# Copyright (C) 2023 The Qt Company Ltd.
# SPDX-License-Identifier: LicenseRef-Qt-Commercial

# Collect test data
file(GLOB_RECURSE test_data_glob
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    testdata/*)
list(APPEND test_data ${test_data_glob})

find_package(Qt6 ${PROJECT_VERSION} CONFIG OPTIONAL_COMPONENTS Widgets UiTools)

qt_internal_add_test(tst_qinsighteventfilter
    SOURCES
        tst_qinsighteventfilter.cpp
    LIBRARIES
        Qt::InsightTracker
        Qt::InsightTrackerPrivate
    TESTDATA ${test_data}
)

qt_internal_extend_target(tst_qinsighteventfilter CONDITION TARGET Qt::Widgets
    PUBLIC_LIBRARIES
        Qt::Widgets
    DEFINES
        QTINSIGHT_HAS_WIDGETS
)

qt_internal_extend_target(tst_qinsighteventfilter CONDITION TARGET Qt::UiTools
    PUBLIC_LIBRARIES
        Qt::UiTools
    DEFINES
        QTINSIGHT_HAS_UITOOLS
)

qt_internal_extend_target(tst_qinsighteventfilter CONDITION TARGET Qt::Quick
    PUBLIC_LIBRARIES
        Qt::Quick
    DEFINES
        QTINSIGHT_HAS_QUICK
)
