import QtQuick
import Nokturn

// Main Play/Pause button: smooth icon swap.
Item {
    id: root
    property real size: 46
    property bool playing: Player.playing
    property bool buffering: Player.buffering && Player.hasTrack
    property color bgColor: Theme.text
    property color fgColor: Theme.bg
    signal clicked

    implicitWidth: size
    implicitHeight: size

    Rectangle {
        id: disc
        anchors.fill: parent
        radius: width / 2
        color: root.bgColor
        scale: mouse.pressed ? 0.9 : mouse.containsMouse ? 1.07 : 1
        Behavior on scale { NumberAnimation { duration: 280; easing.type: Easing.OutBack } }

        Icon {
            anchors.centerIn: parent
            anchors.horizontalCenterOffset: root.size * 0.03
            name: "play"
            size: root.size * 0.5
            color: root.fgColor
            opacity: root.playing || root.buffering ? 0 : 1
            scale: root.playing ? 0.3 : 1
            rotation: root.playing ? -90 : 0
            Behavior on opacity { NumberAnimation { duration: 220 } }
            Behavior on scale { NumberAnimation { duration: 380; easing.type: Easing.OutBack } }
            Behavior on rotation { NumberAnimation { duration: 380; easing.type: Easing.OutCubic } }
        }
        Icon {
            anchors.centerIn: parent
            name: "pause"
            size: root.size * 0.5
            color: root.fgColor
            opacity: root.playing && !root.buffering ? 1 : 0
            scale: root.playing ? 1 : 0.3
            rotation: root.playing ? 0 : 90
            Behavior on opacity { NumberAnimation { duration: 220 } }
            Behavior on scale { NumberAnimation { duration: 380; easing.type: Easing.OutBack } }
            Behavior on rotation { NumberAnimation { duration: 380; easing.type: Easing.OutCubic } }
        }
        Spinner {
            anchors.centerIn: parent
            visible: root.buffering
            size: root.size * 0.42
            color: root.fgColor
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
