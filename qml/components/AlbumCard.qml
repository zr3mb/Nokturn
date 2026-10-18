import QtQuick
import QtQuick.Effects
import Nokturn

// Album / artist card: lifts on hover, the shadow grows and a Play button slides in.
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
    property bool animateIn: true   // false: shown at once (e.g. in a list built after its page appeared)
    // A CardShadow of the same size shared by the whole list; without it the card runs its own effect.
    property CardShadow shadow: null
    signal clicked
    signal playClicked

    readonly property bool hovered: hover.hovered
    property real shadowOpacity: hovered ? 0.6 : 0.38
    property real shadowOffset: hovered ? 16 : 8
    readonly property real shadowPadding: shadow ? shadow.padding : 0
    Behavior on shadowOpacity { NumberAnimation { duration: 400 } }
    Behavior on shadowOffset { NumberAnimation { duration: 400; easing.type: Easing.OutCubic } }

    implicitWidth: 180
    implicitHeight: width + 62
    opacity: animateIn ? 0 : 1
    transform: Translate { id: shift; y: root.animateIn ? 22 : 0 }

    SequentialAnimation {
        id: reveal
        PauseAnimation { duration: root.revealDelay }
        ParallelAnimation {
            NumberAnimation { target: root; property: "opacity"; to: 1; duration: 500; easing.type: Easing.OutCubic }
            NumberAnimation { target: shift; property: "y"; to: 0; duration: 760; easing.type: Easing.OutQuint }
        }
    }
    Component.onCompleted: if (animateIn) reveal.start()

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

        // The list's shared shadow, cut to the area the card's own effect covers.
        Loader {
            active: root.shadow !== null
            x: -root.shadowPadding
            y: -root.shadowPadding
            width: coverBox.width + root.shadowPadding * 2
            height: coverBox.height + root.shadowPadding * 2
            clip: true
            sourceComponent: ShaderEffect {
                property var source: root.shadow
                y: root.shadowOffset
                opacity: root.shadowOpacity
            }
        }

        ArtCover {
            id: coverArt
            anchors.fill: parent
            radius: root.round ? width / 2 : 14
            colorA: root.colorA
            colorB: root.colorB
            seed: root.seed
            source: root.cover
            layer.enabled: root.shadow === null
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowBlur: 1.0
                shadowColor: "#000000"
                shadowOpacity: root.shadowOpacity
                shadowVerticalOffset: root.shadowOffset
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
