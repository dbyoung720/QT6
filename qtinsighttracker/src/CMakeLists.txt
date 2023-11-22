# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: LicenseRef-Qt-Commercial

add_subdirectory(insighttracker)
if(TARGET Qt::Qml)
    add_subdirectory(insighttrackerqml)
endif()
add_subdirectory(plugins)
