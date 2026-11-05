import QtQuick
import Everplay

Item {
    id: root
    property string icon: ""
    property string label: ""
    property string subtitle: ""
    property bool selected: false
    property bool compact: false
    property bool cover: false
    property color colorA: Theme.accent
    property color colorB: Theme.accent2
    property int seed: 0
    property string coverIcon: ""
    property string coverSource: ""
    property var coverMosaic: []
    property bool ownHighlight: false
    property bool playing: false
    property int revealDelay: -1   // >= 0: slides in after this many ms (the menu at startup)
    signal clicked

    transform: Translate { id: enterShift }
    SequentialAnimation {
        id: enterAnim
        PauseAnimation { duration: Theme.ms(Math.max(0, root.revealDelay)) }
        ParallelAnimation {
            NumberAnimation { target: root; property: "opacity"; to: 1; duration: Theme.ms(500); easing.type: Easing.OutCubic }
            NumberAnimation { target: enterShift; property: "x"; to: 0; duration: Theme.ms(700); easing.type: Easing.OutQuint }
        }
    }
    Component.onCompleted: if (revealDelay >= 0) { opacity = 0; enterShift.x = -24; enterAnim.start() }

    width: parent ? parent.width : 220
    height: cover ? 52 : 44

    Rectangle {
        anchors.fill: parent
        radius: Theme.r(14)
        color: root.selected && root.ownHighlight ? Theme.alpha(Theme.accent, 0.16) : Theme.alpha(Theme.text, 0.05)
        opacity: (mouse.containsMouse && !root.selected) || (root.selected && root.ownHighlight) ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Theme.fast } }
    }

    Icon {
        visible: !root.cover
        x: root.compact ? (parent.width - width) / 2 : 15
        anchors.verticalCenter: parent.verticalCenter
        name: root.icon
        size: 21
        color: root.selected ? Theme.accent : mouse.containsMouse ? Theme.text : Theme.textDim
        Behavior on x { NumberAnimation { duration: Theme.ms(460); easing.type: Easing.OutQuint } }
        scale: mouse.pressed ? 0.84 : mouse.containsMouse || root.selected ? 1.12 : 1
        Behavior on color { ColorAnimation { duration: Theme.normal } }
        Behavior on scale { NumberAnimation { duration: Theme.ms(260); easing.type: Easing.OutBack } }
    }

    ArtCover {
        id: art
        visible: root.cover
        x: root.compact ? (parent.width - width) / 2 : 9
        width: 38; height: 38
        anchors.verticalCenter: parent.verticalCenter
        radius: Theme.r(11)
        Behavior on x { NumberAnimation { duration: Theme.ms(460); easing.type: Easing.OutQuint } }
        decorated: false
        colorA: root.colorA
        colorB: root.colorB
        seed: root.seed
        source: root.coverSource
        mosaic: root.coverMosaic
        scale: mouse.pressed ? 0.9 : 1
        Behavior on scale { NumberAnimation { duration: Theme.ms(260); easing.type: Easing.OutBack } }

        Icon {
            anchors.centerIn: parent
            visible: root.coverIcon !== "" && !art.hasPicture
            name: root.coverIcon
            size: 16
            color: "#f4efe8"
        }
    }

    Column {
        anchors {
            left: parent.left; leftMargin: root.cover ? 58 : 50
            right: parent.right; rightMargin: 10
            verticalCenter: parent.verticalCenter
        }
        spacing: 1
        transform: Translate {
            x: mouse.containsMouse && !root.selected ? 5 : 0
            Behavior on x { NumberAnimation { duration: Theme.ms(300); easing.type: Easing.OutCubic } }
        }
        opacity: root.compact ? 0 : 1
        visible: opacity > 0.01
        Behavior on opacity { NumberAnimation { duration: root.compact ? 120 : 380; easing.type: Easing.OutCubic } }

        Text {
            width: parent.width
            text: root.label
            elide: Text.ElideRight
            color: root.selected || root.playing ? (root.playing ? Theme.accent : Theme.text) : Theme.textDim
            font.pixelSize: 14
            font.weight: root.selected ? Font.Bold : Font.Medium
            Behavior on color { ColorAnimation { duration: Theme.normal } }
        }
        Text {
            width: parent.width
            visible: root.subtitle !== ""
            text: root.subtitle
            elide: Text.ElideRight
            color: Theme.textFaint
            font.pixelSize: 12
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
