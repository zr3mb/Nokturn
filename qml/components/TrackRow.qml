import QtQuick
import QtQuick.Layouts
import Nokturn

// Track row in lists: number ↔ play ↔ equalizer, cover, title, album, source, like, duration.
Item {
    id: root
    property int trackIndex: 0
    property int number: trackIndex + 1
    property int revealDelay: 0
    property bool showAlbum: true
    property var queue: null   // rows that become the queue on click (null = the whole library)
    property string context: "" // queue origin ("playlist:…", "album:…") — the mix depends on it

    function play() {
        if (current) Player.toggle()
        else if (queue) Player.playFrom(queue, trackIndex, context)
        else Player.playIndex(trackIndex)
    }

    readonly property var t: {
        Library.tracks.revision
        return Library.tracks.get(trackIndex)
    }
    readonly property bool current: Player.currentIndex === trackIndex
    readonly property bool hovered: hover.hovered

    width: parent ? parent.width : 600
    implicitHeight: 60
    opacity: 0
    transform: Translate { id: shift; y: 16 }

    SequentialAnimation {
        id: reveal
        PauseAnimation { duration: root.revealDelay }
        ParallelAnimation {
            NumberAnimation { target: root; property: "opacity"; to: 1; duration: 420; easing.type: Easing.OutCubic }
            NumberAnimation { target: shift; property: "y"; to: 0; duration: 620; easing.type: Easing.OutQuint }
        }
    }
    Component.onCompleted: reveal.start()

    HoverHandler { id: hover }

    // The cursor rests on a YouTube track for a moment — we prepare its URL before the click lands
    // (the delay keeps fast mouse sweeps over the list from starting yt-dlp for every row).
    Timer {
        interval: 200
        running: hover.hovered && !root.current
        onTriggered: Player.prefetch(root.trackIndex)
    }

    MouseArea {
        anchors.fill: parent
        onDoubleClicked: root.current ? Player.play() : root.play()
    }

    Rectangle {
        anchors.fill: parent
        radius: 12
        color: root.current ? Theme.alpha(Theme.accent, 0.11) : Theme.alpha(Theme.text, 0.055)
        opacity: root.current || root.hovered ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Theme.normal } }
        Behavior on color { ColorAnimation { duration: Theme.normal } }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 10
        spacing: 14

        Item {
            Layout.preferredWidth: 26
            Layout.fillHeight: true

            Text {
                anchors.centerIn: parent
                text: root.number
                color: Theme.textFaint
                font.pixelSize: 14
                font.features: { "tnum": 1 }
                opacity: root.hovered || root.current ? 0 : 1
                Behavior on opacity { NumberAnimation { duration: Theme.fast } }
            }
            EqualizerIcon {
                anchors.centerIn: parent
                visible: root.current && !root.hovered
                running: root.current && Player.playing
            }
            IconButton {
                anchors.centerIn: parent
                width: 34; height: 34
                visible: root.hovered
                icon: root.current && Player.playing ? "pause" : "play"
                iconSize: 18
                color: Theme.text
                showDot: false
                onClicked: root.play()
            }
        }

        ArtCover {
            Layout.preferredWidth: 42
            Layout.preferredHeight: 42
            radius: 8
            colorA: root.t.colorA
            colorB: root.t.colorB
            seed: root.t.seed
            source: root.t.cover || ""
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Text {
                Layout.fillWidth: true
                text: root.t.title
                color: root.current ? Theme.accent : Theme.text
                font.pixelSize: 15
                font.weight: Font.Medium
                elide: Text.ElideRight
                Behavior on color { ColorAnimation { duration: Theme.normal } }
            }
            Text {
                Layout.fillWidth: true
                text: root.t.artist
                color: Theme.textDim
                font.pixelSize: 13
                elide: Text.ElideRight
            }
        }

        Text {
            visible: root.showAlbum
            Layout.preferredWidth: root.width * 0.22
            text: root.t.album
            color: Theme.textDim
            font.pixelSize: 13
            elide: Text.ElideRight
        }

        Item {
            Layout.preferredWidth: 132
            Layout.fillHeight: true
            SourceBadge {
                anchors.verticalCenter: parent.verticalCenter
                source: root.t.source
            }
        }

        IconButton {
            icon: root.t.liked ? "heart" : "heartOutline"
            iconSize: 18
            active: root.t.liked
            showDot: false
            opacity: root.t.liked || root.hovered ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: Theme.fast } }
            onClicked: {
                Library.tracks.toggleLiked(root.trackIndex)
                Nav.toast(root.t.liked ? qsTr("Added to Liked") : qsTr("Removed from Liked"), "heart")
            }
        }

        Text {
            Layout.preferredWidth: 52
            horizontalAlignment: Text.AlignRight
            text: root.t.durationText
            color: root.t.duration > 0 ? Theme.textDim : Theme.warning
            font.pixelSize: root.t.duration > 0 ? 13 : 11
            font.weight: root.t.duration > 0 ? Font.Normal : Font.DemiBold
            font.features: { "tnum": 1 }
        }

        IconButton {
            icon: "more"
            iconSize: 18
            showDot: false
            opacity: root.hovered ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: Theme.fast } }
            onClicked: Nav.toast(qsTr("The context menu is coming soon"), "more")
        }
    }
}
