import QtQuick
import Everplay

// Press feedback: an outline of the button's own shape that spreads out and fades. Several can
// overlap when the button is clicked quickly.
Item {
    id: root
    anchors.fill: parent
    property real radius: Math.min(width, height) / 2
    property color color: Theme.accent
    property real spread: 1.35

    function burst() {
        wave.createObject(root)
    }

    Component {
        id: wave
        Rectangle {
            id: ring
            anchors.fill: parent
            radius: root.radius
            color: "transparent"
            border.width: 2
            border.color: root.color
            opacity: 0.7
            ParallelAnimation {
                running: true
                NumberAnimation { target: ring; property: "scale"; from: 1; to: root.spread; duration: Theme.ms(700); easing.type: Easing.OutCubic }
                NumberAnimation { target: ring; property: "opacity"; to: 0; duration: Theme.ms(700); easing.type: Easing.OutQuad }
                onFinished: ring.destroy()
            }
        }
    }
}
