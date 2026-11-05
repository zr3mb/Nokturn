import QtQuick
import QtQuick.Controls.Basic
import Everplay

// The queue over the full-screen player (Nav.immersiveQueue: "half" or "full", "" = closed). Its header drags
// it between the two heights or down to close it; a click on the header switches them. A click above it
// (ImmersiveView) or the back button closes the queue alone.
Item {
    id: root
    property real topInset: 64           // the full height stops under the top bar
    readonly property string size: Nav.immersiveQueue
    readonly property real halfHeight: Math.round(parent.height * 0.58)
    readonly property real fullHeight: parent.height - topInset
    property real dragHeight: -1         // while the header is dragged and settles

    width: Math.min(parent.width, 760)
    anchors.horizontalCenter: parent.horizontalCenter
    anchors.bottom: parent.bottom
    height: dragHeight >= 0 ? dragHeight : size === "full" ? fullHeight : halfHeight
    Behavior on height {
        enabled: root.dragHeight < 0
        NumberAnimation { duration: Theme.ms(380); easing.type: Easing.OutQuint }
    }

    property real slide: size === "" ? 1 : 0
    Behavior on slide { NumberAnimation { duration: Theme.ms(380); easing.type: Easing.OutQuint } }
    onSlideChanged: if (slide === 1) dragHeight = -1
    transform: Translate { y: root.slide * (root.height + 40) }
    visible: slide < 1

    NumberAnimation {
        id: settle
        target: root
        property: "dragHeight"
        duration: Theme.ms(320)
        easing.type: Easing.OutQuint
        onFinished: root.dragHeight = -1
    }

    Rectangle {
        anchors.fill: parent
        topLeftRadius: 24
        topRightRadius: 24
        color: Theme.surface
        border.color: Theme.panelBorder
    }

    // Keeps clicks and the wheel off the player underneath
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        onWheel: wheel => wheel.accepted = true
    }

    Item {
        id: header
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 78

        MouseArea {
            id: grip
            property real startY: 0
            property real startHeight: 0
            property bool moved: false
            anchors.fill: parent
            cursorShape: pressed && moved ? Qt.ClosedHandCursor : Qt.PointingHandCursor
            onPressed: mouse => {
                settle.stop()
                startY = mapToItem(null, mouse.x, mouse.y).y
                startHeight = root.height
                moved = false
            }
            onPositionChanged: mouse => {
                const d = mapToItem(null, mouse.x, mouse.y).y - startY
                if (Math.abs(d) > 6)
                    moved = true
                if (moved)
                    root.dragHeight = Math.max(0, Math.min(root.fullHeight, startHeight - d))
            }
            onReleased: {
                if (!moved) {
                    Nav.immersiveQueue = root.size === "full" ? "half" : "full"
                    return
                }
                const frac = root.height / root.parent.height
                if (frac < 0.32) {
                    Nav.immersiveQueue = ""
                    return
                }
                Nav.immersiveQueue = frac < 0.75 ? "half" : "full"
                settle.from = root.dragHeight
                settle.to = Nav.immersiveQueue === "full" ? root.fullHeight : root.halfHeight
                settle.restart()
            }
        }

        Rectangle {
            anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 8 }
            width: 40; height: 5; radius: 3
            color: Theme.alpha(Theme.text, grip.containsMouse || grip.pressed ? 0.45 : 0.25)
        }

        Column {
            anchors { left: parent.left; leftMargin: 24; right: buttons.left; rightMargin: 12; verticalCenter: parent.verticalCenter; verticalCenterOffset: 6 }
            spacing: 2
            Text {
                text: qsTr("Queue")
                color: Theme.text
                font.family: Theme.fontDisplay
                font.pixelSize: 21
                font.weight: Font.Bold
            }
            Text {
                width: parent.width
                text: Player.queueLeft > 0
                      ? qsTr("%n tracks", "", Player.queueLeft) + " · " + Theme.formatTotal(Player.queueSeconds)
                      : qsTr("That's the end of the queue")
                color: Theme.textDim
                font.pixelSize: 13
                elide: Text.ElideRight
            }
        }

        Row {
            id: buttons
            anchors { right: parent.right; rightMargin: 14; verticalCenter: parent.verticalCenter; verticalCenterOffset: 6 }
            spacing: 4
            IconButton {
                icon: Player.playing ? "pause" : "play"
                iconSize: 24
                color: Theme.text
                showDot: false
                onClicked: Player.toggle()
            }
            IconButton {
                icon: root.size === "full" ? "expandMore" : "expandLess"
                iconSize: 26
                showDot: false
                onClicked: Nav.immersiveQueue = root.size === "full" ? "half" : "full"
            }
            IconButton {
                icon: "close"
                iconSize: 22
                showDot: false
                onClicked: Nav.immersiveQueue = ""
            }
        }
    }

    ListView {
        id: list
        anchors { left: parent.left; right: parent.right; top: header.bottom; bottom: parent.bottom; leftMargin: 14; rightMargin: 14 }
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: root.visible ? Player.queueAhead : []
        ScrollBar.vertical: ThinScrollBar {}
        Connections {
            target: root
            function onSizeChanged() { if (root.size !== "" && root.slide === 1) list.positionViewAtBeginning() }
        }

        header: Column {
            width: list.width
            SectionLabel { text: qsTr("NOW PLAYING") }
            QueueRow {
                width: parent.width
                current: true
                title: Player.title
                artist: Player.artist
                cover: Player.cover
                colorA: Player.colorA
                colorB: Player.colorB
                seed: Player.seed
                duration: Player.live ? "" : Theme.formatTime(Player.duration)
            }
            SectionLabel { text: qsTr("UP NEXT"); visible: Player.queueLeft > 0 }
        }

        delegate: QueueRow {
            required property var modelData
            readonly property var t: Library.tracks.revision >= 0 ? Library.tracks.get(modelData) : ({})
            width: list.width
            title: t.title || ""
            artist: t.artist || ""
            cover: t.cover || ""
            colorA: t.colorA
            colorB: t.colorB
            seed: t.seed || 0
            duration: t.durationText || ""
            onClicked: Player.jumpTo(modelData)
        }

        footer: Text {
            width: list.width
            topPadding: 12
            bottomPadding: 20
            horizontalAlignment: Text.AlignHCenter
            text: Player.queueLeft > 200 ? qsTr("and %1 more…").arg(Player.queueLeft - 200)
                  : Player.queueLeft === 0 ? qsTr("That's the end of the queue") : ""
            color: Theme.textFaint
            font.pixelSize: 13
        }
    }

    component SectionLabel: Text {
        leftPadding: 10
        topPadding: 14
        bottomPadding: 6
        color: Theme.textFaint
        font.pixelSize: 11
        font.weight: Font.Bold
        font.letterSpacing: 1.5
    }

    component QueueRow: Item {
        id: row
        property bool current: false
        property string title
        property string artist
        property string cover
        property color colorA
        property color colorB
        property int seed
        property string duration
        signal clicked
        implicitHeight: 60

        Rectangle {
            anchors.fill: parent
            anchors.topMargin: 2
            anchors.bottomMargin: 2
            radius: Theme.r(12)
            color: Theme.alpha(Theme.text, row.current ? 0.06 : 0.07)
            opacity: row.current || hover.hovered ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: Theme.fast } }
        }
        HoverHandler { id: hover; enabled: !row.current; cursorShape: Qt.PointingHandCursor }
        TapHandler { enabled: !row.current; onTapped: row.clicked() }

        ArtCover {
            id: art
            x: 10
            width: 44; height: 44
            anchors.verticalCenter: parent.verticalCenter
            radius: Theme.r(8)
            colorA: row.colorA
            colorB: row.colorB
            seed: row.seed
            source: row.cover
            Rectangle {
                anchors.fill: parent
                radius: Theme.r(8)
                color: "#80000000"
                opacity: hover.hovered ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: Theme.fast } }
                Icon { anchors.centerIn: parent; name: "play"; size: 18; color: "#ffffff" }
            }
        }
        Column {
            anchors { left: art.right; leftMargin: 12; right: time.left; rightMargin: 10; verticalCenter: parent.verticalCenter }
            spacing: 2
            Text {
                width: parent.width
                text: row.title
                color: row.current ? Qt.lighter(Theme.accent, 1.15) : Theme.text
                font.pixelSize: 14
                font.weight: Font.Medium
                elide: Text.ElideRight
            }
            Text {
                width: parent.width
                text: row.artist
                color: Theme.textDim
                font.pixelSize: 12
                elide: Text.ElideRight
            }
        }
        Text {
            id: time
            anchors { right: parent.right; rightMargin: 10; verticalCenter: parent.verticalCenter }
            text: row.duration
            color: Theme.textFaint
            font.pixelSize: 12
            font.features: { "tnum": 1 }
        }
    }
}
