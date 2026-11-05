import QtQuick
import Everplay

// Pill-shaped button: "primary" | "secondary" | "ghost".
Item {
    id: root
    property string text: ""
    property string icon: ""
    property string kind: "primary"
    property bool busy: false
    property real fontSize: 14
    signal clicked

    readonly property color fg: kind === "primary" ? Theme.onAccent : Theme.text

    implicitHeight: 44
    implicitWidth: row.implicitWidth + 40
    opacity: enabled ? 1 : 0.45

    PressRing {
        id: ring
        radius: bg.radius
        color: root.kind === "primary" ? Theme.accent : Theme.text
        spread: 1.12
    }

    Rectangle {
        id: bg
        anchors.fill: parent
        radius: height / 2
        color: root.kind === "primary"
               ? (mouse.containsMouse ? Qt.lighter(Theme.accent, 1.1) : Theme.accent)
               : root.kind === "secondary"
                 ? Theme.alpha(Theme.text, mouse.containsMouse ? 0.15 : 0.09)
                 : Theme.alpha(Theme.text, mouse.containsMouse ? 0.07 : 0)
        border.width: root.kind === "primary" ? 0 : 1
        border.color: Theme.alpha(Theme.text, root.kind === "ghost" ? (mouse.containsMouse ? 0.28 : 0.14) : 0.08)
        scale: mouse.pressed ? 0.96 : 1
        Behavior on color { ColorAnimation { duration: Theme.normal } }
        Behavior on scale { NumberAnimation { duration: Theme.ms(220); easing.type: Easing.OutBack } }
    }

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 8

        Spinner {
            anchors.verticalCenter: parent.verticalCenter
            visible: root.busy
            size: 16
            color: root.fg
        }
        Icon {
            anchors.verticalCenter: parent.verticalCenter
            visible: root.icon !== "" && !root.busy
            name: root.icon
            size: 18
            color: root.fg
            rotation: mouse.containsMouse ? -8 : 0
            scale: mouse.containsMouse ? 1.12 : 1
            Behavior on rotation { NumberAnimation { duration: Theme.ms(320); easing.type: Easing.OutBack } }
            Behavior on scale { NumberAnimation { duration: Theme.ms(320); easing.type: Easing.OutBack } }
            rotation: mouse.containsMouse ? -8 : 0
            scale: mouse.containsMouse ? 1.12 : 1
            Behavior on rotation { NumberAnimation { duration: Theme.ms(320); easing.type: Easing.OutBack } }
            Behavior on scale { NumberAnimation { duration: Theme.ms(320); easing.type: Easing.OutBack } }
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.text
            color: root.fg
            font.pixelSize: root.fontSize
            font.weight: Font.Bold
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        enabled: !root.busy
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            ring.burst()
            root.clicked()
        }
    }
}
