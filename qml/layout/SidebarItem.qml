import QtQuick
import Nokturn

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
    signal clicked

    width: parent ? parent.width : 220
    height: cover ? 54 : 44

    Rectangle {
        anchors.fill: parent
        radius: 12
        color: Theme.alpha(Theme.text, root.selected && root.ownHighlight ? 0.08 : 0.045)
        opacity: (mouse.containsMouse && !root.selected) || (root.selected && root.ownHighlight) ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Theme.fast } }
    }

    Icon {
        visible: !root.cover
        x: 15
        anchors.verticalCenter: parent.verticalCenter
        name: root.icon
        size: 22
        color: root.selected || mouse.containsMouse ? Theme.text : Theme.textDim
        scale: mouse.pressed ? 0.84 : 1
        Behavior on color { ColorAnimation { duration: Theme.normal } }
        Behavior on scale { NumberAnimation { duration: 260; easing.type: Easing.OutBack } }
    }

    ArtCover {
        id: art
        visible: root.cover
        x: 10
        width: 34; height: 34
        anchors.verticalCenter: parent.verticalCenter
        radius: 9
        decorated: false
        colorA: root.colorA
        colorB: root.colorB
        seed: root.seed
        source: root.coverSource
        mosaic: root.coverMosaic
        scale: mouse.pressed ? 0.9 : 1
        Behavior on scale { NumberAnimation { duration: 260; easing.type: Easing.OutBack } }

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
            left: parent.left; leftMargin: root.cover ? 56 : 50
            right: parent.right; rightMargin: 10
            verticalCenter: parent.verticalCenter
        }
        spacing: 1
        opacity: root.compact ? 0 : 1
        visible: opacity > 0.01
        Behavior on opacity { NumberAnimation { duration: root.compact ? 120 : 380; easing.type: Easing.OutCubic } }

        Text {
            width: parent.width
            text: root.label
            elide: Text.ElideRight
            color: root.selected || root.playing ? (root.playing ? Theme.accent : Theme.text) : Theme.textDim
            font.pixelSize: 14
            font.weight: root.selected ? Font.DemiBold : Font.Medium
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
