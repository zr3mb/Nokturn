import QtQuick
import QtQuick.Effects
import Nokturn

// Karta albumu / wykonawcy: unosi się po najechaniu, cień rośnie, wysuwa się przycisk Play.
Item {
    id: root
    property string title: ""
    property string subtitle: ""
    property color colorA: Theme.accent
    property color colorB: Theme.accent2
    property int seed: 0
    property string cover: ""
    property bool round: false
    property bool playing: false
    property int revealDelay: 0
    signal clicked
    signal playClicked

    readonly property bool hovered: hover.hovered

    implicitWidth: 180
    implicitHeight: width + 62
    opacity: 0
    transform: Translate { id: shift; y: 22 }

    SequentialAnimation {
        id: reveal
        PauseAnimation { duration: root.revealDelay }
        ParallelAnimation {
            NumberAnimation { target: root; property: "opacity"; to: 1; duration: 500; easing.type: Easing.OutCubic }
            NumberAnimation { target: shift; property: "y"; to: 0; duration: 760; easing.type: Easing.OutQuint }
        }
    }
    Component.onCompleted: reveal.start()

    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: root.clicked() }

    Rectangle {
        anchors.fill: parent
        anchors.margins: -10
        radius: 18
        color: Theme.alpha(Theme.text, 0.05)
        opacity: root.hovered ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Theme.normal } }
    }

    Item {
        id: coverBox
        width: root.width
        height: root.width
        y: root.hovered ? -5 : 0
        Behavior on y { NumberAnimation { duration: 420; easing.type: Easing.OutCubic } }

        ArtCover {
            id: coverArt
            anchors.fill: parent
            radius: root.round ? width / 2 : 14
            colorA: root.colorA
            colorB: root.colorB
            seed: root.seed
            source: root.cover
            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowBlur: 1.0
                shadowColor: "#000000"
                shadowOpacity: root.hovered ? 0.6 : 0.38
                shadowVerticalOffset: root.hovered ? 16 : 8
                Behavior on shadowOpacity { NumberAnimation { duration: 400 } }
                Behavior on shadowVerticalOffset { NumberAnimation { duration: 400; easing.type: Easing.OutCubic } }
            }
        }

        PlayFab {
            size: 46
            playing: root.playing
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.rightMargin: root.round ? parent.width * 0.08 : 10
            anchors.bottomMargin: root.hovered || root.playing ? 10 : -4
            opacity: root.hovered || root.playing ? 1 : 0
            Behavior on anchors.bottomMargin { NumberAnimation { duration: 380; easing.type: Easing.OutBack } }
            Behavior on opacity { NumberAnimation { duration: 260 } }
            onClicked: root.playClicked()
        }
    }

    Column {
        anchors { left: parent.left; right: parent.right; top: coverBox.bottom; topMargin: 14 }
        spacing: 3

        Text {
            width: parent.width
            text: root.title
            color: root.playing ? Theme.accent : Theme.text
            font.pixelSize: 15
            font.weight: Font.DemiBold
            elide: Text.ElideRight
            horizontalAlignment: root.round ? Text.AlignHCenter : Text.AlignLeft
        }
        Text {
            width: parent.width
            text: root.subtitle
            color: Theme.textDim
            font.pixelSize: 13
            elide: Text.ElideRight
            horizontalAlignment: root.round ? Text.AlignHCenter : Text.AlignLeft
        }
    }
}
