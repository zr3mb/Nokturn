import QtQuick
import Nokturn

// A compact playlist row (small cover, name, Play on hover): the shortcuts on the home page and the
// playlist panel next to the search. playlist: a Library.playlists entry (or one shaped like it).
Item {
    id: root
    property var playlist: ({})
    signal clicked
    signal playClicked

    implicitWidth: 230
    implicitHeight: 34

    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: root.clicked() }

    Rectangle {
        anchors.fill: parent
        radius: 8
        color: Theme.alpha(Theme.text, hover.hovered ? 0.06 : 0)
        Behavior on color { ColorAnimation { duration: Theme.fast } }
    }
    ArtCover {
        id: cover
        x: 4
        anchors.verticalCenter: parent.verticalCenter
        width: 26
        height: 26
        radius: 7
        decorated: false
        colorA: root.playlist.colorA || Theme.accent
        colorB: root.playlist.colorB || Theme.accent2
        seed: root.playlist.seed || 0
        source: root.playlist.cover || ""
        mosaic: root.playlist.mosaic || []
        Icon {
            anchors.centerIn: parent
            visible: !cover.hasPicture
            name: root.playlist.icon || "music"
            size: 15
            color: "#f4efe8"
        }
    }
    Text {
        anchors { left: cover.right; leftMargin: 12; right: play.left; rightMargin: 6; verticalCenter: parent.verticalCenter }
        text: root.playlist.name || ""
        color: Theme.text
        font.pixelSize: 13
        font.weight: Font.Medium
        elide: Text.ElideRight
    }
    IconButton {
        id: play
        anchors { right: parent.right; rightMargin: 2; verticalCenter: parent.verticalCenter }
        implicitWidth: 30
        implicitHeight: 30
        icon: "play"
        iconSize: 16
        showDot: false
        opacity: hover.hovered ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Theme.fast } }
        onClicked: root.playClicked()
    }
}
