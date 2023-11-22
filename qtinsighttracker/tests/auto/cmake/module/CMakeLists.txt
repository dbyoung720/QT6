# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: LicenseRef-Qt-Commercial

cmake_minimum_required(VERSION 3.16)

project(tst_module)

find_package(Qt5 REQUIRED COMPONENTS Gui Network Sql InsightTracker)
find_package(Qt5 CONFIG OPTIONAL_COMPONENTS Sql)

add_executable(mainapp main.cpp)

target_link_libraries(mainapp
    Qt::InsightTracker
)
