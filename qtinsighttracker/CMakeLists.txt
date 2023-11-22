# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: LicenseRef-Qt-Commercial

cmake_minimum_required(VERSION 3.16)

include(.cmake.conf)
project(QtInsightTracker
    VERSION "${QT_REPO_MODULE_VERSION}"
    DESCRIPTION "Qt Insight Tracker Libraries"
    HOMEPAGE_URL "https://qt.io/"
    LANGUAGES CXX C
)

set(QT_NO_PACKAGE_VERSION_CHECK TRUE)
set(QT_NO_PACKAGE_VERSION_INCOMPATIBLE_WARNING TRUE)

find_package(Qt6 ${PROJECT_VERSION} CONFIG REQUIRED COMPONENTS BuildInternals Core Network)
find_package(Qt6 ${PROJECT_VERSION} CONFIG OPTIONAL_COMPONENTS Gui Sql Qml Quick QuickTemplates2)

if(NOT TARGET Qt::Gui)
    message(NOTICE "Skipping the build as the condition \"TARGET Qt::Gui\" is not met.")
    return()
endif()

qt_build_repo()
