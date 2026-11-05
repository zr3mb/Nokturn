import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import Everplay

// The home page banner: the current cover, blurred, fills the card; the track, the controls and the
// cover itself sit on top of it.
Panel {
    id: root
    implicitHeight: 300
    Layout.fillWidth: true

    Connections {
        target: Player
        function onCurrentChanged() { change.restart() }
    }
    ParallelAnimation {
        id: change
        NumberAnimation { target: flip; property: "angle"; from: 90; to: 0; duration: Theme.ms(900); easing.type: Easing.OutBack }
        NumberAnimation { target: titleShift; property: "y"; from: 30; to: 0; duration: Theme.ms(800); easing.type: Easing.OutQuint }
        NumberAnimation { target: titleBlock; property: "opacity"; from: 0; to: 1; duration: Theme.ms(500); easing.type: Easing.OutCubic }
    }
    radius: Theme.radiusXL
    tint: Theme.alpha(Theme.surfaceHi, 0.6)

    // The cover, large and soft, as the backdrop
    ArtCover {
        id: backdrop
        anchors.centerIn: parent
        width: parent.width * 1.25
        height: width
        radius: Theme.r(0)
        decorated: false
        colorA: Player.colorA
        colorB: Player.colorB
        seed: Player.seed
        source: Player.cover
        visible: false
    }
    MultiEffect {
        anchors.fill: backdrop
        source: backdrop
        blurEnabled: true
        blur: 1.0
        blurMax: 64
        saturation: 0.25
        opacity: 0.75
    }

    // Shade: dark on the text side, lighter where the cover sits
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: Theme.alpha(Theme.bg, 0.86) }
            GradientStop { position: 0.55; color: Theme.alpha(Theme.bg, 0.55) }
            GradientStop { position: 1.0; color: Theme.alpha(Theme.bg, 0.25) }
        }
    }
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.5; color: "#00000000" }
            GradientStop { position: 1.0; color: Theme.alpha(Theme.bg, 0.5) }
        }
    }

    Visualizer {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        bars: 64
        spacing: 3
        barWidth: (width - spacing * (bars - 1)) / bars
        height: 44
        gain: 0.7
        colorTop: Theme.alpha(Theme.accent, 0.22)
        colorBottom: Theme.alpha(Theme.accent, 0.0)
        opacity: Player.playing ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Theme.ms(600) } }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 34
        spacing: 34

        ColumnLayout {
            id: titleBlock
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 100   // the cover keeps its size; long titles shrink instead
            spacing: 8
            transform: Translate { id: titleShift }

            // Status chip
            Rectangle {
                implicitWidth: chip.implicitWidth + 22
                implicitHeight: 28
                radius: 14
                color: Theme.alpha(Theme.accent, 0.18)
                border.width: 1
                border.color: Theme.alpha(Theme.accent, 0.35)
                Row {
                    id: chip
                    anchors.centerIn: parent
                    spacing: 8
                    EqualizerIcon {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: Player.playing
                        running: Player.playing
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: Player.playing ? qsTr("NOW PLAYING")
                            : Player.hasTrack ? qsTr("CONTINUE LISTENING")
                            : Library.localCount > 0 ? qsTr("READY TO PLAY") : qsTr("START HERE")
                        color: Theme.accent
                        font.pixelSize: 11
                        font.weight: Font.Bold
                        font.letterSpacing: 1.6
                    }
                }
            }

            Item { Layout.fillHeight: true }

            Text {
                Layout.fillWidth: true
                text: Player.hasTrack ? Player.title
                      : Library.localCount > 0 ? qsTr("Your library is waiting") : qsTr("Add your music")
                color: Theme.text
                font.family: Theme.fontDisplay
                font.pixelSize: 46
                font.weight: Font.Bold
                font.letterSpacing: -1.2
                fontSizeMode: Text.HorizontalFit
                minimumPixelSize: 30
                elide: Text.ElideRight
                maximumLineCount: 1
            }
            Text {
                Layout.fillWidth: true
                text: Player.hasTrack ? Player.artist + (Player.album ? "  ·  " + Player.album : "")
                      : Library.scanning ? Library.scanStatus
                      : Library.localCount > 0
                        ? qsTr("%n tracks from your folders", "", Library.localCount)
                        : qsTr("Point to a folder with files — Everplay will read the tags and covers.")
                color: Theme.alpha(Theme.text, 0.72)
                font.pixelSize: 16
                font.weight: Font.Medium
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }

            Item { Layout.preferredHeight: 10 }

            RowLayout {
                visible: !Player.hasTrack
                spacing: 12

                PillButton {
                    icon: Library.localCount > 0 ? "play" : "folder"
                    text: Library.localCount > 0 ? qsTr("Play everything") : qsTr("Add a music folder")
                    onClicked: Library.localCount > 0 ? Player.play() : Nav.addFolder()
                }
                PillButton {
                    visible: Library.localCount > 1
                    kind: "secondary"
                    icon: "shuffle"
                    text: qsTr("Shuffle")
                    onClicked: Player.playShuffled(Library.libraryRows(), "library")
                }
            }

            RowLayout {
                visible: Player.hasTrack
                spacing: 14

                PlayButton {
                    size: 56
                    bgColor: Theme.accent
                    fgColor: Theme.onAccent
                    onClicked: Player.toggle()
                }
                PillButton {
                    kind: "secondary"
                    icon: "expand"
                    text: qsTr("Full screen")
                    onClicked: Nav.immersive = true
                }
                IconButton {
                    icon: Player.liked ? "heart" : "heartOutline"
                    active: Player.liked
                    showDot: false
                    implicitWidth: 44; implicitHeight: 44
                    onClicked: {
                        Player.toggleLiked()
                        Nav.toast(Player.liked ? qsTr("Added to Liked") : qsTr("Removed from Liked"), "heart")
                    }
                }
            }
        }

        ArtCover {
            id: cover
            Layout.preferredWidth: root.height - 68
            Layout.preferredHeight: root.height - 68
            Layout.alignment: Qt.AlignVCenter
            visible: root.width > 640
            radius: Theme.radiusL
            colorA: Player.colorA
            colorB: Player.colorB
            seed: Player.seed
            source: Player.cover
            scale: 1 + Player.level * 0.03
            transform: [
                Rotation {
                    id: flip
                    origin.x: cover.width / 2
                    origin.y: cover.height / 2
                    axis { x: 0; y: 1; z: 0 }
                },
                Translate { id: bob }
            ]
            SequentialAnimation {
                running: Player.playing
                loops: Animation.Infinite
                NumberAnimation { target: bob; property: "y"; to: -7; duration: 2600; easing.type: Easing.InOutSine }
                NumberAnimation { target: bob; property: "y"; to: 4; duration: 2600; easing.type: Easing.InOutSine }
            }
            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowBlur: 1.0
                shadowOpacity: 0.65
                shadowVerticalOffset: 16
            }

            HoverHandler { id: coverHover; cursorShape: Player.hasTrack ? Qt.PointingHandCursor : Qt.ArrowCursor }
            TapHandler { enabled: Player.hasTrack; onTapped: Nav.immersive = true }
            Rectangle {
                anchors.fill: parent
                radius: Theme.radiusL
                color: "#66000000"
                opacity: coverHover.hovered && Player.hasTrack ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: Theme.fast } }
                Icon { anchors.centerIn: parent; name: "expand"; size: 30; color: "#ffffff" }
            }
        }
    }
}
