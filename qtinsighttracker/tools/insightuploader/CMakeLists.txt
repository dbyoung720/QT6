# Copyright (C) 2023 The Qt Company Ltd.
# SPDX-License-Identifier: LicenseRef-Qt-Commercial

qt_internal_add_app(insightuploader
    TARGET_DESCRIPTION "Insight Offline Uploader"
    SOURCES
        insightuploader.cpp
    DEFINES
        QT_NO_CAST_FROM_ASCII
        QT_NO_CAST_TO_ASCII
    LIBRARIES
        Qt::InsightTrackerPrivate
)

set_target_properties(insightuploader PROPERTIES WIN32_EXECUTABLE FALSE)
