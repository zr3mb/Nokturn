import QtQuick
import Nokturn

// Progress/volume slider: thickens on hover, shows a handle and a value bubble.
Item {
    id: root
    property real value: 0                 // 0..1
    property color fillColor: Theme.text
    property color activeColor: Theme.accent
    property var labelFor: null            // function(v) -> string; bubble above the cursor
    readonly property bool active: enabled && (mouse.containsMouse || mouse.pressed)
    property real dragValue: 0
    readonly property real shown: mouse.pressed ? dragValue : Math.max(0, Math.min(1, value))
    signal dragging(real value)
    signal moved(real value)

    implicitWidth: 200
    implicitHeight: 18

    function valueAt(x) { return Math.max(0, Math.min(1, x / width)) }

    Rectangle {
        id: track
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width
        height: root.active ? 6 : 4
        radius: height / 2
        color: Theme.alpha(Theme.text, 0.12)
        Behavior on height { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

        Rectangle {
            width: Math.max(height, parent.width * root.shown)
            height: parent.height
            radius: height / 2
            visible: root.shown > 0
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0; color: root.active ? Qt.darker(root.activeColor, 1.15) : Theme.alpha(root.fillColor, 0.75) }
                GradientStop { position: 1; color: root.active ? root.activeColor : root.fillColor }
            }
        }
    }

    Rectangle {
        id: handleGlow
        width: 26; height: 26; radius: 13
        x: track.width * root.shown - width / 2
        anchors.verticalCenter: parent.verticalCenter
        color: Theme.alpha(root.activeColor, 0.25)
        scale: mouse.pressed ? 1 : 0
        Behavior on scale { NumberAnimation { duration: 240; easing.type: Easing.OutBack } }
    }

    Rectangle {
        id: handle
        width: 14; height: 14; radius: 7
        x: track.width * root.shown - width / 2
        anchors.verticalCenter: parent.verticalCenter
        color: Theme.text
        scale: root.active ? 1 : 0
        Behavior on scale { NumberAnimation { duration: 260; easing.type: Easing.OutBack } }
    }

    // Value bubble at the cursor position
    Rectangle {
        id: bubble
        visible: root.labelFor !== null
        readonly property real at: mouse.pressed ? root.dragValue : root.valueAt(mouse.mouseX)
        width: bubbleText.implicitWidth + 14
        height: 22
        radius: 6
        x: Math.max(-6, Math.min(root.width - width + 6, root.width * at - width / 2))
        y: -height - 8
        color: Theme.surfaceHi
        border.color: Qt.rgba(1, 1, 1, 0.07)
        opacity: root.active ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Theme.fast } }

        Text {
            id: bubbleText
            anchors.centerIn: parent
            text: root.labelFor ? root.labelFor(bubble.at) : ""
            color: Theme.text
            font.pixelSize: 11
            font.features: { "tnum": 1 }
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        anchors.topMargin: -6
        anchors.bottomMargin: -6
        hoverEnabled: true
        preventStealing: true
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onPressed: mouse => {
            root.dragValue = root.valueAt(mouse.x)
            root.dragging(root.dragValue)
        }
        onPositionChanged: mouse => {
            if (pressed) {
                root.dragValue = root.valueAt(mouse.x)
                root.dragging(root.dragValue)
            }
        }
        onReleased: root.moved(root.dragValue)
    }
}
