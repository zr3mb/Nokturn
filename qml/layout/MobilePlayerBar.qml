import QtQuick
import Nokturn

// Phone layout: the mini player above the bottom navigation. Tapping it opens the full-screen player.
Item {
    id: root
    visible: Player.hasTrack

    Rectangle {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        radius: 14
        color: Theme.alpha(Theme.surface, 0.96)
        border.width: 1
        border.color: Theme.alpha(Theme.text, 0.06)
        clip: true

        MouseArea {
            anchors.fill: parent
            onClicked: Nav.immersive = true
        }

        ArtCover {
            id: cover
            x: 8
            width: 44; height: 44
            anchors.verticalCenter: parent.verticalCenter
            radius: 9
            colorA: Player.colorA
            colorB: Player.colorB
            seed: Player.seed
            source: Player.cover
        }

        Column {
            anchors {
                left: cover.right; leftMargin: 12
                right: controls.left; rightMargin: 8
                verticalCenter: parent.verticalCenter
            }
            spacing: 2
            Text {
                width: parent.width
                text: Player.title
                color: Theme.text
                font.pixelSize: 14
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            Text {
                width: parent.width
                text: Player.artist
                color: Theme.textDim
                font.pixelSize: 12
                elide: Text.ElideRight
            }
        }

        Row {
            id: controls
            anchors { right: parent.right; rightMargin: 6; verticalCenter: parent.verticalCenter }
            spacing: 2
            IconButton {
                width: 42; height: 42
                icon: Player.liked ? "heart" : "heartOutline"
                active: Player.liked
                onClicked: Player.toggleLiked()
            }
            PlayButton {
                size: 40
                anchors.verticalCenter: parent.verticalCenter
                onClicked: Player.toggle()
            }
            IconButton {
                width: 42; height: 42
                icon: "next"
                onClicked: Player.next()
            }
        }

        // Progress along the bottom edge
        Rectangle {
            anchors { left: parent.left; bottom: parent.bottom; leftMargin: 10; rightMargin: 10 }
            height: 2
            radius: 1
            width: (parent.width - 20) * (Player.live ? 1 : Math.min(1, Player.position / Math.max(1, Player.duration)))
            color: Theme.accent
        }
    }
}
