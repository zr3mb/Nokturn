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
    property bool halo: true   // a glow around the button that follows the loudness while playing
    signal clicked

    implicitWidth: size
    implicitHeight: size

    Rectangle {
        anchors.fill: parent
        visible: root.halo
        radius: width / 2
        color: root.bgColor
        opacity: root.playing ? 0.22 : 0
        scale: root.playing ? 1.08 + Player.level * 0.38 : 1
        Behavior on opacity { NumberAnimation { duration: Theme.ms(400) } }
        Behavior on scale { NumberAnimation { duration: 90 } }
    }

    PressRing {
        id: ring
        color: root.bgColor
        spread: 1.6
    }

    Rectangle {
        id: disc
        anchors.fill: parent
        radius: width / 2
        color: root.bgColor
        scale: mouse.pressed ? 0.9 : mouse.containsMouse ? 1.07 : 1
        Behavior on scale { NumberAnimation { duration: Theme.ms(280); easing.type: Easing.OutBack } }

        Icon {
            anchors.centerIn: parent
            anchors.horizontalCenterOffset: root.size * 0.03
            name: "play"
            size: root.size * 0.5
            color: root.fgColor
            opacity: root.playing || root.buffering ? 0 : 1
            scale: root.playing ? 0.3 : 1
            rotation: root.playing ? -90 : 0
            Behavior on opacity { NumberAnimation { duration: Theme.ms(220) } }
            Behavior on scale { NumberAnimation { duration: Theme.ms(380); easing.type: Easing.OutBack } }
            Behavior on rotation { NumberAnimation { duration: Theme.ms(380); easing.type: Easing.OutCubic } }
        }
        Icon {
            anchors.centerIn: parent
            name: "pause"
            size: root.size * 0.5
            color: root.fgColor
            opacity: root.playing && !root.buffering ? 1 : 0
            scale: root.playing ? 1 : 0.3
            rotation: root.playing ? 0 : 90
            Behavior on opacity { NumberAnimation { duration: Theme.ms(220) } }
            Behavior on scale { NumberAnimation { duration: Theme.ms(380); easing.type: Easing.OutBack } }
            Behavior on rotation { NumberAnimation { duration: Theme.ms(380); easing.type: Easing.OutCubic } }
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
        onClicked: {
            ring.burst()
            root.clicked()
        }
    }
}
