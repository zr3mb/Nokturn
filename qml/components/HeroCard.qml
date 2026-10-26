import QtQuick
import QtQuick.Layouts
import QtQuick.Shapes
import QtQuick.Effects
import Nokturn

// Big "Continue listening" card with a visualizer in the background.
Item {
    id: root
    implicitHeight: Nav.mobile ? 200 : 236
    Layout.fillWidth: true

    readonly property string frame: Theme.roundedRectPath(width, height, 24)

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: Qt.rgba(1, 1, 1, 0.06)
            strokeWidth: 1
            fillGradient: LinearGradient {
                x1: 0; y1: 0; x2: root.width; y2: root.height
                GradientStop { position: 0; color: Theme.alpha(Theme.accent, 0.3) }
                GradientStop { position: 0.55; color: Theme.alpha(Theme.accent, 0.16) }
                GradientStop { position: 1; color: Theme.alpha(Theme.surfaceHi, 0.5) }
            }
            PathSvg { path: root.frame }
        }
    }

    Visualizer {
        visible: !Nav.mobile
        anchors { right: parent.right; bottom: parent.bottom; rightMargin: 26; bottomMargin: 24 }
        bars: 22
        barWidth: 4
        spacing: 3
        height: 84
        width: implicitWidth
        colorTop: Theme.alpha(Theme.text, 0.22)
        colorBottom: Theme.alpha(Theme.text, 0.02)
        opacity: Player.playing ? 1 : 0.0
        Behavior on opacity { NumberAnimation { duration: 600 } }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: Nav.mobile ? 18 : 26
        spacing: Nav.mobile ? 16 : 26

        ArtCover {
            id: cover
            Layout.alignment: Qt.AlignTop
            Layout.preferredWidth: Nav.mobile ? 84 : root.height - 52
            Layout.preferredHeight: Nav.mobile ? 84 : root.height - 52
            radius: 16
            colorA: Player.colorA
            colorB: Player.colorB
            seed: Player.seed
            source: Player.cover
            scale: 1 + Player.level * 0.02
            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowBlur: 1.0
                shadowOpacity: 0.55
                shadowVerticalOffset: 12
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 6

            RowLayout {
                Layout.fillWidth: true
                Text {
                    Layout.fillWidth: true
                    text: Player.playing ? qsTr("NOW PLAYING")
                        : Player.hasTrack ? qsTr("CONTINUE LISTENING")
                        : Library.localCount > 0 ? qsTr("READY TO PLAY") : qsTr("START HERE")
                    color: Qt.lighter(Theme.accent, 1.15)
                    font.pixelSize: 11
                    font.weight: Font.Bold
                    font.letterSpacing: 2.2
                }
                PillButton {
                    kind: "ghost"
                    icon: "expand"
                    text: qsTr("Full screen")
                    fontSize: 13
                    implicitHeight: 34
                    visible: Player.hasTrack && !Nav.mobile
                    onClicked: Nav.immersive = true
                }
            }
            Text {
                Layout.fillWidth: true
                text: Player.hasTrack ? Player.title
                      : Library.localCount > 0 ? qsTr("Your library is waiting") : qsTr("Add your music")
                color: Theme.text
                font.family: Theme.fontDisplay
                font.pixelSize: Nav.mobile ? 24 : 34
                font.weight: Font.Bold
                elide: Text.ElideRight
                wrapMode: Nav.mobile ? Text.Wrap : Text.NoWrap
                maximumLineCount: 2
            }
            Text {
                Layout.fillWidth: true
                text: Player.hasTrack ? Player.artist + "  ·  " + Player.album
                      : Library.scanning ? Library.scanStatus
                      : Library.localCount > 0
                        ? qsTr("%n tracks from your folders", "", Library.localCount)
                        : qsTr("Point to a folder with files — Nokturn will read the tags and covers.")
                color: Theme.textDim
                font.pixelSize: Nav.mobile ? 13 : 15
                elide: Text.ElideRight
                wrapMode: Nav.mobile ? Text.Wrap : Text.NoWrap
                maximumLineCount: Nav.mobile ? 3 : 1
            }
            SourceBadge {
                Layout.topMargin: 4
                source: Player.source
                visible: Player.hasTrack
            }

            Item { Layout.fillHeight: true }

            RowLayout {
                visible: !Player.hasTrack
                spacing: 12

                PillButton {
                    icon: Library.localCount > 0 ? "play" : "folder"
                    text: Library.localCount > 0 ? qsTr("Play everything") : qsTr("Add a music folder")
                    onClicked: Library.localCount > 0 ? Player.play() : Nav.addFolder()
                }
                PillButton {
                    visible: Library.localCount > 1 && !Nav.mobile
                    kind: "secondary"
                    icon: "shuffle"
                    text: qsTr("Shuffle")
                    onClicked: Player.playShuffled(Library.libraryRows(), "library")
                }
            }

            RowLayout {
                visible: Player.hasTrack
                spacing: 16

                PlayButton {
                    size: 52
                    bgColor: Theme.accent
                    fgColor: Theme.onAccent
                    onClicked: Player.toggle()
                }

                ColumnLayout {
                    spacing: 6
                    Layout.preferredWidth: 220

                    Rectangle {
                        Layout.fillWidth: true
                        height: 4
                        radius: 2
                        color: Theme.alpha(Theme.text, 0.14)
                        Rectangle {
                            width: parent.width * (Player.live ? 1 : Player.position / Math.max(1, Player.duration))
                            height: parent.height
                            radius: 2
                            color: Theme.text
                        }
                    }
                    Text {
                        text: Player.live ? qsTr("Live broadcast")
                                          : Theme.formatTime(Player.position) + " / " + Theme.formatTime(Player.duration)
                        color: Theme.textDim
                        font.pixelSize: 12
                        font.features: { "tnum": 1 }
                    }
                }

            }
        }
    }
}
