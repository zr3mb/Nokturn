import QtQuick
import Everplay

Item {
    id: root
    property string icon: ""
    property real iconSize: 20
    property color color: Theme.textDim
    property color hoverColor: Theme.text
    property bool active: false
    property color activeColor: Theme.accent
    property bool showDot: false   // the tinted circle marks an active button
    property bool popOnActivate: true
    readonly property bool hovered: mouse.containsMouse
    signal clicked

    implicitWidth: 38
    implicitHeight: 38
    opacity: enabled ? 1 : 0.35

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: root.active ? root.activeColor : Theme.text
        opacity: mouse.pressed ? 0.16 : mouse.containsMouse ? 0.09 : root.active ? 0.1 : 0
        Behavior on opacity { NumberAnimation { duration: Theme.fast } }
    }

    PressRing {
        id: ring
        color: root.active ? root.activeColor : Theme.text
        spread: 1.45
    }

    Icon {
        id: glyph
        anchors.centerIn: parent
        name: root.icon
        size: root.iconSize
        color: root.active ? root.activeColor : mouse.containsMouse ? root.hoverColor : root.color
        scale: mouse.pressed ? 0.84 : 1
        Behavior on color { ColorAnimation { duration: Theme.normal } }
        Behavior on scale { NumberAnimation { duration: Theme.ms(260); easing.type: Easing.OutBack } }
    }

    Rectangle {
        visible: root.showDot
        width: 4; height: 4; radius: 2
        anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 1 }
        color: root.activeColor
        opacity: root.active ? 1 : 0
        scale: root.active ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Theme.normal } }
        Behavior on scale { NumberAnimation { duration: Theme.ms(320); easing.type: Easing.OutBack } }
    }

    SequentialAnimation {
        id: pop
        NumberAnimation { target: glyph; property: "scale"; to: 1.35; duration: Theme.ms(140); easing.type: Easing.OutCubic }
        NumberAnimation { target: glyph; property: "scale"; to: 1.0; duration: Theme.ms(420); easing.type: Easing.OutElastic; easing.amplitude: 1.2; easing.period: 0.5 }
    }
    onActiveChanged: if (active && popOnActivate) pop.restart()

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
