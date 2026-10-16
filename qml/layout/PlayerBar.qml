import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import Nokturn

// Dolny pasek odtwarzacza.
Item {
    id: root

    Connections {
        target: Player
        function onCurrentChanged() { swap.restart() }
    }

    ParallelAnimation {
        id: swap
        NumberAnimation { target: cover; property: "scale"; from: 0.8; to: 1; duration: 560; easing.type: Easing.OutBack }
        NumberAnimation { target: info; property: "opacity"; from: 0; to: 1; duration: 420; easing.type: Easing.OutCubic }
        NumberAnimation { target: infoShift; property: "x"; from: 16; to: 0; duration: 600; easing.type: Easing.OutQuint }
    }

    // ---- Lewa część: bieżący utwór -------------------------------------------
    Item {
        id: left
        anchors { left: parent.left; leftMargin: 22; verticalCenter: parent.verticalCenter }
        width: Math.min(360, root.width * 0.27)
        height: 64

        ArtCover {
            id: cover
            width: 60; height: 60
            anchors.verticalCenter: parent.verticalCenter
            radius: 11
            colorA: Player.colorA
            colorB: Player.colorB
            seed: Player.seed
            source: Player.cover
            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowBlur: 0.8
                shadowOpacity: 0.5
                shadowVerticalOffset: 4
            }

            Rectangle {
                anchors.fill: parent
                radius: 11
                color: "#80000000"
                opacity: coverMouse.containsMouse ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: Theme.fast } }
                Icon {
                    anchors.centerIn: parent
                    name: "expand"
                    size: 20
                    color: "#ffffff"
                }
            }
            MouseArea {
                id: coverMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: Nav.immersive = true
            }
        }

        Column {
            id: info
            anchors {
                left: cover.right; leftMargin: 14
                right: likeBtn.left; rightMargin: 6
                verticalCenter: parent.verticalCenter
            }
            spacing: 4
            transform: Translate { id: infoShift }

            Text {
                width: parent.width
                text: Player.title
                color: Theme.text
                font.pixelSize: 15
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            Row {
                width: parent.width
                spacing: 8
                SourceBadge {
                    anchors.verticalCenter: parent.verticalCenter
                    source: Player.source
                    visible: Player.hasTrack
                    showLabel: false
                    implicitHeight: 20
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - 40
                    text: Player.artist
                    color: Theme.textDim
                    font.pixelSize: 13
                    elide: Text.ElideRight
                }
            }
        }

        IconButton {
            id: likeBtn
            anchors { right: parent.right; verticalCenter: parent.verticalCenter }
            icon: Player.liked ? "heart" : "heartOutline"
            active: Player.liked
            enabled: Player.hasTrack
            showDot: false
            onClicked: {
                Player.toggleLiked()
                Nav.toast(Player.liked ? "Dodano do Ulubionych" : "Usunięto z Ulubionych", "heart")
            }
        }
    }

    // ---- Środek: sterowanie i postęp ---------------------------------------
    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(640, root.width * 0.4)
        spacing: 6

        Row {
            Layout.alignment: Qt.AlignHCenter
            spacing: 14

            IconButton {
                anchors.verticalCenter: parent.verticalCenter
                icon: "shuffle"
                active: Player.shuffle
                onClicked: Player.shuffle = !Player.shuffle
            }
            IconButton {
                anchors.verticalCenter: parent.verticalCenter
                icon: "prev"
                iconSize: 26
                color: Theme.text
                showDot: false
                onClicked: Player.previous()
            }
            PlayButton {
                anchors.verticalCenter: parent.verticalCenter
                size: 46
                onClicked: Player.toggle()
            }
            IconButton {
                anchors.verticalCenter: parent.verticalCenter
                icon: "next"
                iconSize: 26
                color: Theme.text
                showDot: false
                onClicked: Player.next()
            }
            IconButton {
                anchors.verticalCenter: parent.verticalCenter
                icon: Player.repeatMode === 2 ? "repeatOne" : "repeat"
                active: Player.repeatMode > 0
                onClicked: Player.cycleRepeat()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Text {
                Layout.preferredWidth: 44
                horizontalAlignment: Text.AlignRight
                text: Player.live ? "" : Theme.formatTime(Player.position)
                color: Theme.textDim
                font.pixelSize: 12
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
                Layout.preferredWidth: 44
                text: Player.live ? "NA ŻYWO" : Theme.formatTime(Player.duration)
                color: Player.live ? Theme.warning : Theme.textDim
                font.pixelSize: Player.live ? 11 : 12
                font.weight: Player.live ? Font.DemiBold : Font.Normal
                font.features: { "tnum": 1 }
            }
        }
    }

    // ---- Prawa część: wizualizer, kolejka, głośność -------------------------
    Row {
        anchors { right: parent.right; rightMargin: 22; verticalCenter: parent.verticalCenter }
        spacing: 4

        Visualizer {
            anchors.verticalCenter: parent.verticalCenter
            bars: 12
            barWidth: 3
            spacing: 2
            height: 24
            width: implicitWidth
            minHeight: 3
            opacity: Player.playing ? 1 : 0.35
            Behavior on opacity { NumberAnimation { duration: 500 } }
        }
        Item { width: 12; height: 1 }
        IconButton {
            anchors.verticalCenter: parent.verticalCenter
            icon: "equalizer"
            active: Nav.page === "equalizer" || (Equalizer.enabled && Equalizer.preset !== "Płasko")
            onClicked: Nav.navigate("equalizer")
        }
        IconButton {
            anchors.verticalCenter: parent.verticalCenter
            icon: "queue"
            active: Nav.queueOpen
            onClicked: Nav.queueOpen = !Nav.queueOpen
        }
        IconButton {
            anchors.verticalCenter: parent.verticalCenter
            icon: "expand"
            iconSize: 18
            onClicked: Nav.immersive = true
        }
        IconButton {
            anchors.verticalCenter: parent.verticalCenter
            icon: Player.muted || Player.volume === 0 ? "volumeOff" : Player.volume < 0.5 ? "volumeDown" : "volumeUp"
            showDot: false
            onClicked: Player.muted = !Player.muted
        }
        ProgressSlider {
            anchors.verticalCenter: parent.verticalCenter
            width: 112
            value: Player.muted ? 0 : Player.volume
            labelFor: function(v) { return Math.round(v * 100) + "%" }
            onDragging: v => { Player.muted = false; Player.volume = v }
            onMoved: v => { Player.muted = false; Player.volume = v }
        }
    }
}
