import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import Nokturn

// Full-screen "Now playing" mode: big cover, a stronger background, a wide visualizer.
Item {
    id: root
    readonly property bool open: Nav.immersive
    // Everything but the cover is built only while the view shows (or fades out); the cover stays
    // loaded, so its picture is there the moment the view opens.
    readonly property bool built: open || visible

    visible: opacity > 0.01
    opacity: open ? 1 : 0
    Behavior on opacity { NumberAnimation { duration: Theme.ms(460); easing.type: Easing.OutCubic } }

    // Short windows (a small second screen, a laptop): type, buttons and gaps shrink with the height
    readonly property real k: Math.max(0.62, Math.min(1, (height - 64) / 760))

    property real enter: open ? 0 : 1
    Behavior on enter { NumberAnimation { duration: Theme.ms(800); easing.type: Easing.OutQuint } }

    Rectangle {
        anchors.fill: parent
        color: Theme.bg
    }

    Loader {
        anchors.fill: parent
        active: root.built
        sourceComponent: AmbientBackground { boost: 2.0 }
    }

    // Blocks clicks to the layer underneath
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.AllButtons
        onWheel: wheel => wheel.accepted = true
        // the mouse's back button closes the queue, then leaves full screen
        onPressed: mouse => {
            if (mouse.button === Qt.BackButton)
                Nav.immersiveQueue !== "" ? Nav.immersiveQueue = "" : Nav.immersive = false
        }
    }

    Loader {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; leftMargin: 24; rightMargin: 24 }
        height: 150 * root.k
        active: root.built
        sourceComponent: Visualizer {
            bars: 80
            spacing: 4
            barWidth: (width - spacing * (bars - 1)) / bars
            symmetric: true
            gain: 1.05
            colorTop: Theme.alpha(Theme.accent, 0.5)
            colorBottom: Theme.alpha(Theme.accent, 0.0)
            opacity: 1 - root.enter
        }
    }

    // Top bar
    Loader {
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 64
        active: root.built
        sourceComponent: Item {
            DragHandler {
                target: null
                onActiveChanged: if (active) root.Window.window.startSystemMove()
            }

            IconButton {
                x: 22
                anchors.verticalCenter: parent.verticalCenter
                icon: "expandMore"
                iconSize: 28
                showDot: false
                onClicked: Nav.immersive = false
            }

            Column {
                anchors.centerIn: parent
                spacing: 2
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("PLAYING FROM")
                    color: Theme.textFaint
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    font.letterSpacing: 2
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Math.min(implicitWidth, root.width - 200)
                    text: Player.album || (Player.source === "youtube" ? "YouTube Music" : Player.artist)
                    color: Theme.text
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }
            }

            IconButton {
                anchors { right: lyricsButton.left; rightMargin: 4; verticalCenter: parent.verticalCenter }
                icon: "queue"
                iconSize: 22
                active: Nav.immersiveQueue !== ""
                onClicked: Nav.immersiveQueue = Nav.immersiveQueue !== "" ? "" : "half"
            }

            IconButton {
                id: lyricsButton
                anchors { right: windowControls.left; rightMargin: 12; verticalCenter: parent.verticalCenter }
                icon: "lyrics"
                iconSize: 22
                active: Nav.immersiveLyrics
                onClicked: Nav.immersiveLyrics = !Nav.immersiveLyrics
            }

            WindowControls {
                id: windowControls
                anchors { right: parent.right; rightMargin: 10; verticalCenter: parent.verticalCenter }
            }
        }
    }

    readonly property bool showLyrics: Nav.immersiveLyrics && root.width >= 640 && Lyrics.state !== "none"
    // Narrow windows (a small second screen): a smaller cover, controls and lyrics squeeze side by side;
    // below 900 px the lyrics take the cover's place
    readonly property bool narrowLyrics: showLyrics && root.width <= 1100
    readonly property bool lyricsOverCover: showLyrics && root.width < 900

    // The room between the top bar and "Up next"
    Item {
        id: stage
        anchors { left: parent.left; right: parent.right; top: parent.top; bottom: parent.bottom
                  topMargin: 64; bottomMargin: Player.hasTrack ? 100 : 24 }
    }

    RowLayout {
        anchors.centerIn: stage
        spacing: Math.min(root.showLyrics ? 60 : 90, root.width * (root.narrowLyrics ? 0.03 : 0.05))

        Item {
            id: coverBox
            visible: !root.lyricsOverCover
            readonly property real s: Math.min(stage.height * 0.7, root.height * 0.52, root.width * (root.narrowLyrics ? 0.2 : root.showLyrics ? 0.24 : 0.34), 500)
            Layout.preferredWidth: s
            Layout.preferredHeight: s
            transform: Translate { y: root.enter * 60 }
            scale: 0.92 + (1 - root.enter) * 0.08

            ArtCover {
                anchors.fill: parent
                radius: 26
                colorA: Player.colorA
                colorB: Player.colorB
                seed: Player.seed
                source: Player.cover
                scale: 1 + Player.level * 0.025
                layer.enabled: root.visible   // the shadow's offscreen textures are released while the view is closed
                layer.effect: MultiEffect {
                    shadowEnabled: true
                    shadowBlur: 1.0
                    shadowOpacity: 0.7
                    shadowVerticalOffset: 26
                }
            }
        }

        Loader {
            Layout.preferredWidth: Math.min(540, root.width * (root.lyricsOverCover ? 0.42 : root.narrowLyrics ? 0.38 : root.showLyrics ? 0.3 : 0.4))
            active: root.built
            sourceComponent: ColumnLayout {
                spacing: 10 * root.k
                opacity: 1 - root.enter
                transform: Translate { x: root.enter * 40 }

                Text {
                    visible: root.height >= 640
                    text: Player.playing ? qsTr("NOW PLAYING") : qsTr("PAUSED")
                    color: Qt.lighter(Theme.accent, 1.15)
                    font.pixelSize: 12
                    font.weight: Font.Bold
                    font.letterSpacing: 3
                }
                Text {
                    Layout.fillWidth: true
                    text: Player.title
                    color: Theme.text
                    font.family: Theme.fontDisplay
                    font.pixelSize: Math.round(56 * root.k)
                    font.weight: Font.Bold
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                    lineHeight: 0.95
                }
                ArtistLink {
                    Layout.fillWidth: true
                    text: Player.artist
                    artistId: Player.artistId
                    linkEnabled: Player.hasTrack
                    font.pixelSize: Math.max(16, Math.round(22 * root.k))
                    elide: Text.ElideRight
                }
                RowLayout {
                    spacing: 10
                    SourceBadge { source: Player.source; visible: Player.hasTrack }
                    Text {
                        text: Player.quality
                        color: Theme.textFaint
                        font.pixelSize: 12
                    }
                }

                Item { Layout.preferredHeight: 22 * root.k }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text {
                        text: Player.live ? "" : Theme.formatTime(Player.position)
                        color: Theme.textDim
                        font.pixelSize: 13
                        font.features: { "tnum": 1 }
                    }
                    ProgressSlider {
                        Layout.fillWidth: true
                        enabled: !Player.live
                        value: Player.live ? 1 : Player.position / Math.max(1, Player.duration)
                        labelFor: function(v) { return Theme.formatTime(v * Player.duration) }
                        onMoved: v => Player.seek(v * Player.duration)
                    }
                    Text {
                        text: Player.live ? qsTr("LIVE") : Theme.formatTime(Player.duration)
                        color: Player.live ? Theme.warning : Theme.textDim
                        font.pixelSize: 13
                        font.features: { "tnum": 1 }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 8 * root.k
                    spacing: 18

                    IconButton {
                        icon: "shuffle"
                        iconSize: 22
                        active: Player.shuffle
                        onClicked: Player.shuffle = !Player.shuffle
                    }
                    IconButton {
                        icon: "prev"
                        iconSize: 32
                        implicitWidth: 48; implicitHeight: 48
                        color: Theme.text
                        showDot: false
                        onClicked: Player.previous()
                    }
                    PlayButton {
                        size: Math.max(54, Math.round(70 * root.k))
                        onClicked: Player.toggle()
                    }
                    IconButton {
                        icon: "next"
                        iconSize: 32
                        implicitWidth: 48; implicitHeight: 48
                        color: Theme.text
                        showDot: false
                        onClicked: Player.next()
                    }
                    IconButton {
                        icon: Player.repeatMode === 2 ? "repeatOne" : "repeat"
                        iconSize: 22
                        active: Player.repeatMode > 0
                        onClicked: Player.cycleRepeat()
                    }
                    Item { Layout.fillWidth: true }
                    IconButton {
                        icon: Player.liked ? "heart" : "heartOutline"
                        iconSize: 24
                        active: Player.liked
                        enabled: Player.hasTrack
                        showDot: false
                        onClicked: {
                            Player.toggleLiked()
                            Nav.toast(Player.liked ? qsTr("Added to Liked") : qsTr("Removed from Liked"), "heart")
                        }
                    }
                }

                RowLayout {
                    spacing: 8
                    Layout.topMargin: 4
                    IconButton {
                        icon: Player.muted || Player.volume === 0 ? "volumeOff" : Player.volume < 0.5 ? "volumeDown" : "volumeUp"
                        showDot: false
                        onClicked: Player.muted = !Player.muted
                    }
                    ProgressSlider {
                        Layout.preferredWidth: 160
                        value: Player.muted ? 0 : Player.volume
                        labelFor: function(v) { return Math.round(v * 100) + "%" }
                        onDragging: v => { Player.muted = false; Player.volume = v }
                        onMoved: v => { Player.muted = false; Player.volume = v }
                    }
                }
            }
        }

        Loader {
            visible: root.showLyrics
            Layout.preferredWidth: Math.min(520, root.width * (root.lyricsOverCover ? 0.44 : root.narrowLyrics ? 0.32 : 0.3))
            Layout.preferredHeight: Math.min(root.narrowLyrics ? stage.height : root.height * 0.66, 640)
            active: root.built && root.showLyrics
            sourceComponent: LyricsView {
                opacity: 1 - root.enter
                transform: Translate { x: root.enter * 60 }
                large: true
                fontSize: Math.max(20, Math.min(30, root.width / 56))
                lineSpacing: 14
            }
        }
    }

    Loader {
        anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 24 }
        width: Math.min(440, root.width - 48)
        active: root.built && Player.hasTrack
        sourceComponent: UpNextBar { opacity: 1 - root.enter }
    }

    // The queue, over the player
    Loader {
        anchors.fill: parent
        active: root.built
        sourceComponent: Item {
            // A click above the open queue closes it (the top bar stays usable)
            MouseArea {
                anchors { fill: parent; topMargin: 64 }
                enabled: Nav.immersiveQueue !== ""
                acceptedButtons: Qt.AllButtons
                onClicked: Nav.immersiveQueue = ""
                onWheel: wheel => wheel.accepted = true

                Rectangle {
                    anchors.fill: parent
                    color: "#000000"
                    opacity: Nav.immersiveQueue !== "" ? 0.35 : 0
                    Behavior on opacity { NumberAnimation { duration: Theme.ms(380); easing.type: Easing.OutQuint } }
                }
            }
            QueueDrawer {}
        }
    }
}
