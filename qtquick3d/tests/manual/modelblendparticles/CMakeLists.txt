# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: BSD-3-Clause

qt_internal_add_manual_test(manual_test_modelblendparticles
    GUI
    SOURCES
        main.cpp
    LIBRARIES
        Qt::Gui
        Qt::Quick
        Qt::Quick3D
)

qt_add_qml_module(manual_test_modelblendparticles
    URI ModelBlendParticlesExample
    VERSION 1.0
    QML_FILES
        main.qml
        SettingsView.qml
        CustomSelectionBox.qml
        CustomSlider.qml
        CustomCheckBox.qml
    RESOURCES
        images/icon_pause.png
        images/icon_play.png
        images/icon_settings.png
    SOURCES
        testgeometry.h
    NO_RESOURCE_TARGET_PATH
    IMPORTS
        QtQuick3D
)

