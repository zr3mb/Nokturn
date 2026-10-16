import QtQuick
import Nokturn

Item {
    id: root
    property bool checked: false
    signal toggled(bool checked)

    implicitWidth: 46
    implicitHeight: 26

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: root.checked ? Theme.accent : Theme.alpha(Theme.text, mouse.containsMouse ? 0.16 : 0.11)
        Behavior on color { ColorAnimation { duration: Theme.normal } }
    }

    Rectangle {
        id: knob
        width: mouse.pressed ? 24 : 20
        height: 20
        radius: 10
        y: 3
        x: root.checked ? root.width - width - 3 : 3
        color: root.checked ? "#15161a" : Theme.text
        Behavior on x { NumberAnimation { duration: 340; easing.type: Easing.OutBack; easing.overshoot: 1.6 } }
        Behavior on width { NumberAnimation { duration: 160 } }
        Behavior on color { ColorAnimation { duration: Theme.normal } }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            root.checked = !root.checked
            root.toggled(root.checked)
        }
    }
}
