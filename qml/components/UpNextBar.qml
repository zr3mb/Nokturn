import QtQuick
import Nokturn

// The full-screen player's "Up next": the next track; a click or a swipe up opens the queue.
Item {
    id: root
    readonly property var next: Player.upNext.length > 0 && Library.tracks.revision >= 0 ? Library.tracks.get(Player.upNext[0]) : null
    implicitHeight: 64

    function open() { Nav.immersiveQueue = "half" }

    Rectangle {
        anchors.fill: parent
        radius: 16
        color: Theme.alpha(Theme.text, mouse.pressed ? 0.13 : mouse.containsMouse ? 0.1 : 0.07)
        Behavior on color { ColorAnimation { duration: Theme.fast } }
    }

    ArtCover {
        id: art
        visible: !!root.next
        x: 10
        width: 44; height: 44
        anchors.verticalCenter: parent.verticalCenter
        radius: 8
        colorA: root.next ? root.next.colorA : Theme.surface
        colorB: root.next ? root.next.colorB : Theme.surface
        seed: root.next ? root.next.seed : 0
        source: root.next ? root.next.cover || "" : ""
    }
    Icon {
        visible: !root.next
        x: 22
        anchors.verticalCenter: parent.verticalCenter
        name: "queue"
        size: 22
        color: Theme.textDim
    }

    Column {
        anchors { left: art.right; leftMargin: 12; right: chevron.left; rightMargin: 10; verticalCenter: parent.verticalCenter }
        spacing: 1
        Text {
            text: root.next ? qsTr("UP NEXT") : qsTr("QUEUE")
            color: Theme.textFaint
            font.pixelSize: 10
            font.weight: Font.Bold
            font.letterSpacing: 1.5
        }
        Text {
            width: parent.width
            text: root.next ? root.next.title : qsTr("That's the end of the queue")
            color: Theme.text
            font.pixelSize: 14
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }
        Text {
            visible: !!root.next
            width: parent.width
            text: root.next ? root.next.artist : ""
            color: Theme.textDim
            font.pixelSize: 12
            elide: Text.ElideRight
        }
    }

    Icon {
        id: chevron
        anchors { right: parent.right; rightMargin: 16; verticalCenter: parent.verticalCenter }
        name: "expandLess"
        size: 24
        color: mouse.containsMouse ? Theme.text : Theme.textDim
    }

    MouseArea {
        id: mouse
        property real startY: -1
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onPressed: mouse => startY = mouse.y
        onPositionChanged: mouse => {
            if (startY >= 0 && startY - mouse.y > 24) {
                startY = -1
                root.open()
            }
        }
        onClicked: root.open()
    }
}
