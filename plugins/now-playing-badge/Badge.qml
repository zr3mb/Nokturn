import QtQuick
import Nokturn

// An overlay: drawn above the whole interface and never in the way of the mouse.
// When a new track starts, a small card with its cover and title slides in at the top for a moment.
Item {
    id: root

    Connections {
        target: Player
        function onCurrentChanged() {
            if (!Player.hasTrack)
                return
            badge.show = true
            hide.restart()
        }
    }
    Timer {
        id: hide
        interval: 4000
        onTriggered: badge.show = false
    }

    Rectangle {
        id: badge
        property bool show: false
        anchors.horizontalCenter: parent.horizontalCenter
        y: show ? 70 : -height - 10
        Behavior on y { NumberAnimation { duration: 520; easing.type: Easing.OutBack } }
        opacity: show ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 300 } }
        width: Math.min(420, row.implicitWidth + 24)
        height: 64
        radius: 32
        color: Qt.rgba(Theme.surfaceHi.r, Theme.surfaceHi.g, Theme.surfaceHi.b, 0.92)
        border.width: 1
        border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.4)

        Row {
            id: row
            x: 10
            anchors.verticalCenter: parent.verticalCenter
            spacing: 12
            // ArtCover: Nokturn's own cover component (falls back to a gradient when there's no picture)
            ArtCover {
                width: 44; height: 44
                radius: 22
                source: Player.cover
                colorA: Player.colorA
                colorB: Player.colorB
                seed: Player.seed
            }
            Column {
                anchors.verticalCenter: parent.verticalCenter
                width: Math.min(330, Math.max(titleText.implicitWidth, artistText.implicitWidth))
                Text {
                    id: titleText
                    width: parent.width
                    text: Player.title
                    color: Theme.text
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }
                Text {
                    id: artistText
                    width: parent.width
                    text: Player.artist
                    color: Theme.textDim
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }
            }
        }
    }
}
