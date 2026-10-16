import QtQuick
import QtQuick.Effects
import Nokturn

// Round play button shown on cards on hover.
Item {
    id: root
    property real size: 46
    property bool playing: false
    signal clicked

    implicitWidth: size
    implicitHeight: size

    Rectangle {
        id: disc
        anchors.fill: parent
        radius: width / 2
        color: mouse.containsMouse ? Qt.lighter(Theme.accent, 1.1) : Theme.accent
        scale: mouse.pressed ? 0.9 : mouse.containsMouse ? 1.08 : 1
        Behavior on scale { NumberAnimation { duration: 260; easing.type: Easing.OutBack } }
        Behavior on color { ColorAnimation { duration: Theme.fast } }

        layer.enabled: true
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
            color: "#15161a"
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
