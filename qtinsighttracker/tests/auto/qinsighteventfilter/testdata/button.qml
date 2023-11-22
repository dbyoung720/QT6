import QtQuick
import QtQuick.Controls

Button {
    visible: true
    id: button
    text: "Hello Insight"
    objectName: "HelloButton"

    Shortcut {
        sequences: [ StandardKey.Copy ]
    }
}
