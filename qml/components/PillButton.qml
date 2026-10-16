import QtQuick
import Nokturn

// Przycisk w kształcie pigułki: "primary" | "secondary" | "ghost".
Item {
    id: root
    property string text: ""
    property string icon: ""
    property string kind: "primary"
    property bool busy: false
    property real fontSize: 14
    signal clicked

    readonly property color fg: kind === "primary" ? "#15161a" : Theme.text

    implicitHeight: 40
    implicitWidth: row.implicitWidth + 36
    opacity: enabled ? 1 : 0.45

    Rectangle {
        id: bg
        anchors.fill: parent
        radius: height / 2
        color: root.kind === "primary"
               ? (mouse.containsMouse ? Qt.lighter(Theme.accent, 1.1) : Theme.accent)
               : root.kind === "secondary"
                 ? Theme.alpha(Theme.text, mouse.containsMouse ? 0.13 : 0.08)
                 : Theme.alpha(Theme.text, mouse.containsMouse ? 0.06 : 0)
        border.width: root.kind === "ghost" ? 1 : 0
        border.color: Theme.alpha(Theme.text, mouse.containsMouse ? 0.3 : 0.16)
        scale: mouse.pressed ? 0.96 : 1
        Behavior on color { ColorAnimation { duration: Theme.normal } }
        Behavior on scale { NumberAnimation { duration: 220; easing.type: Easing.OutBack } }
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
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.text
            color: root.fg
            font.pixelSize: root.fontSize
            font.weight: Font.DemiBold
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        enabled: !root.busy
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
