# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: LicenseRef-Qt-Commercial

qt_internal_add_qml_module(InsightTrackerQml
    VERSION ${PROJECT_VERSION}
    URI QtInsightTracker
    SOURCES
        qinsighttrackerqml.cpp qinsighttrackerqml_p.h
        qinsightcategory_p.h
        qtinsighttrackerqmlexports.h
    DEFINES
        QT_BUILD_INSIGHTTRACKERQML_LIB
        QT_NO_CAST_FROM_ASCII
        QT_NO_CAST_FROM_BYTEARRAY
        QT_NO_CAST_TO_ASCII
        QT_NO_URL_CAST_FROM_STRING
    PUBLIC_LIBRARIES
        Qt::Core
        Qt::Qml
        Qt::Quick
        Qt::InsightTrackerPrivate
)
