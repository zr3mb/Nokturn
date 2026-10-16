import QtQuick
import Nokturn

// Kompaktowy kafelek "szybkiego wyboru" na stronie głównej.
Item {
    id: root
    property string title: ""
    property color colorA: Theme.accent
    property color colorB: Theme.accent2
    property int seed: 0
    property string coverIcon: ""
    property bool playing: false
    property int revealDelay: 0
    signal clicked
    signal playClicked

    implicitHeight: 68
    opacity: 0
    scale: 0.96

    SequentialAnimation {
        id: reveal
        PauseAnimation { duration: root.revealDelay }
        ParallelAnimation {
            NumberAnimation { target: root; property: "opacity"; to: 1; duration: 420; easing.type: Easing.OutCubic }
            NumberAnimation { target: root; property: "scale"; to: 1; duration: 560; easing.type: Easing.OutBack }
        }
    }
    Component.onCompleted: reveal.start()

    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: root.clicked() }

    Rectangle {
        anchors.fill: parent
        radius: 14
        color: Theme.alpha(Theme.text, hover.hovered ? 0.1 : 0.055)
        border.width: 1
        border.color: Theme.alpha(Theme.text, hover.hovered ? 0.08 : 0.03)
        Behavior on color { ColorAnimation { duration: Theme.normal } }
    }

    ArtCover {
        id: cover
        width: root.height
        height: root.height
        radius: 14
        decorated: false
        colorA: root.colorA
        colorB: root.colorB
        seed: root.seed

        Icon {
            anchors.centerIn: parent
            visible: root.coverIcon !== ""
            name: root.coverIcon
            size: 26
            color: "#f4efe8"
        }
        EqualizerIcon {
            anchors.centerIn: parent
            visible: root.playing && root.coverIcon === ""
            running: root.playing
            color: "#f4efe8"
        }
    }

    Text {
        anchors {
            left: cover.right; leftMargin: 16
            right: fab.left; rightMargin: 10
            verticalCenter: parent.verticalCenter
        }
        text: root.title
        color: root.playing ? Theme.accent : Theme.text
        font.pixelSize: 15
        font.weight: Font.DemiBold
        elide: Text.ElideRight
    }

    PlayFab {
        id: fab
        size: 38
        playing: root.playing
        anchors.right: parent.right
        anchors.rightMargin: 14
        anchors.verticalCenter: parent.verticalCenter
        opacity: hover.hovered || root.playing ? 1 : 0
        scale: hover.hovered || root.playing ? 1 : 0.6
        Behavior on opacity { NumberAnimation { duration: 220 } }
        Behavior on scale { NumberAnimation { duration: 340; easing.type: Easing.OutBack } }
        onClicked: root.playClicked()
    }
}
