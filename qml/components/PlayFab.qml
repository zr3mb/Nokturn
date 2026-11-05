import QtQuick
import QtQuick.Effects
import Everplay

// Round play button shown on cards on hover.
Item {
    id: root
    property real size: 46
    property bool playing: false
    // The row this button starts: its stream URL is prepared while the cursor rests on the button
    property int prefetchRow: -1
    signal clicked

    implicitWidth: size
    implicitHeight: size

    PressRing { id: ring }

    Rectangle {
        id: disc
        anchors.fill: parent
        radius: width / 2
        color: mouse.containsMouse ? Qt.lighter(Theme.accent, 1.1) : Theme.accent
        scale: mouse.pressed ? 0.9 : mouse.containsMouse ? 1.08 : 1
        Behavior on scale { NumberAnimation { duration: Theme.ms(260); easing.type: Easing.OutBack } }
        Behavior on color { ColorAnimation { duration: Theme.fast } }

        layer.enabled: root.opacity > 0   // cards show the button on hover: no shadow textures while hidden
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowBlur: 0.6
            shadowOpacity: 0.5
            shadowVerticalOffset: 4
        }

        Icon {
            anchors.centerIn: parent
            anchors.horizontalCenterOffset: root.playing ? 0 : root.size * 0.03
            name: root.playing ? "pause" : "play"
            size: root.size * 0.48
            color: Theme.onAccent
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onPressed: if (root.prefetchRow >= 0 && !root.playing) Player.prefetch(root.prefetchRow)
        onClicked: {
            ring.burst()
            root.clicked()
        }
    }

    Timer {
        interval: 200
        running: mouse.containsMouse && root.prefetchRow >= 0 && !root.playing
        onTriggered: Player.prefetch(root.prefetchRow)
    }
}
