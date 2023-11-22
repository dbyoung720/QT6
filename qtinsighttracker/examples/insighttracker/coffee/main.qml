// Copyright (C) 2017 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Coffee
import QtInsightTracker

ApplicationWindow {
    visible: true
    width: Constants.width
    height: Constants.height
    title: qsTr("Coffee")

    //! [0]
    InsightConfiguration {
        syncInterval: 60
    }

    Component.onCompleted: InsightTracker.enabled = true;
    //! [0]

    ApplicationFlow {
    }
}
