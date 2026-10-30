import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nokturn

// Adding a playlist to the listening room: all of it or tracks "from"–"to", in order or shuffled, at the
// end of the room's queue or right after the playing track.
Popup {
    id: root
    property var rows: []            // Library rows, in the playlist's order
    property string title: ""
    property int from: 1             // 1-based, inclusive
    property int to: 1
    property bool shuffled: false
    property bool next: false

    readonly property int first: Math.max(1, Math.min(from, rows.length))
    readonly property int last: Math.max(first, Math.min(to, rows.length))
    readonly property int count: rows.length > 0 ? last - first + 1 : 0
    readonly property int limit: 500   // a room's queue

    function show(list, name) {
        rows = list || []
        title = name || ""
        from = 1
        to = rows.length
        shuffled = false
        next = false
        fromField.text = "1"
        toField.text = String(rows.length)
        open()
    }

    function trackTitle(n) {
        if (Library.tracks.revision < 0 || n < 1 || n > rows.length)
            return ""
        const t = Library.tracks.get(rows[n - 1])
        return t ? t.title || "" : ""
    }

    function add() {
        let picked = rows.slice(first - 1, last)
        if (shuffled) {
            for (let i = picked.length - 1; i > 0; --i) {
                const j = Math.floor(Math.random() * (i + 1))
                const x = picked[i]; picked[i] = picked[j]; picked[j] = x
            }
        }
        Online.addToRoom(picked, next)
        close()
    }

    anchors.centerIn: Overlay.overlay
    width: Math.min(480, (Overlay.overlay ? Overlay.overlay.width : 520) - 40)
    modal: true
    focus: true
    padding: 24
    background: Rectangle {
        radius: 22
        color: Theme.surfaceHi
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.07)
    }
    Overlay.modal: Rectangle { color: "#88000000" }
    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 200 }
        NumberAnimation { property: "scale"; from: 0.95; to: 1; duration: 320; easing.type: Easing.OutBack }
    }
    exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 160 } }

    component Label: Text {
        color: Theme.textFaint
        font.pixelSize: 11
        font.weight: Font.DemiBold
        font.letterSpacing: 1.2
    }

    contentItem: ColumnLayout {
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            Icon { name: "group"; size: 22; color: Theme.accent }
            Text {
                Layout.fillWidth: true
                text: qsTr("Add to the listening room")
                color: Theme.text
                font.pixelSize: 18
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
        }
        Text {
            Layout.fillWidth: true
            visible: root.title !== ""
            text: root.title
            color: Theme.textDim
            font.pixelSize: 13
            elide: Text.ElideRight
        }

        // which tracks
        Label { text: qsTr("TRACKS"); Layout.topMargin: 4 }
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            InputField {
                id: fromField
                Layout.preferredWidth: 96
                horizontalAlignment: TextInput.AlignHCenter
                inputMethodHints: Qt.ImhDigitsOnly
                validator: IntValidator { bottom: 1; top: Math.max(1, root.rows.length) }
                onTextEdited: if (text !== "") root.from = parseInt(text)
                onEditingFinished: text = String(root.first)
                onAccepted: root.add()
            }
            Text { text: "–"; color: Theme.textDim; font.pixelSize: 16 }
            InputField {
                id: toField
                Layout.preferredWidth: 96
                horizontalAlignment: TextInput.AlignHCenter
                inputMethodHints: Qt.ImhDigitsOnly
                validator: IntValidator { bottom: 1; top: Math.max(1, root.rows.length) }
                onTextEdited: if (text !== "") root.to = parseInt(text)
                onEditingFinished: text = String(root.last)
                onAccepted: root.add()
            }
            Text {
                Layout.fillWidth: true
                text: qsTr("of %1").arg(root.rows.length)
                color: Theme.textDim
                font.pixelSize: 13
            }
            PillButton {
                kind: "ghost"
                text: qsTr("All")
                fontSize: 13
                implicitHeight: 34
                visible: root.first !== 1 || root.last !== root.rows.length
                onClicked: {
                    root.from = 1
                    root.to = root.rows.length
                    fromField.text = "1"
                    toField.text = String(root.rows.length)
                }
            }
        }
        Text {
            Layout.fillWidth: true
            text: root.count === 1 ? root.trackTitle(root.first)
                                   : qsTr("%1 … %2").arg(root.trackTitle(root.first)).arg(root.trackTitle(root.last))
            color: Theme.textFaint
            font.pixelSize: 12
            elide: Text.ElideMiddle
        }

        Label { text: qsTr("ORDER"); Layout.topMargin: 4 }
        SegmentedTabs {
            model: [qsTr("As in the playlist"), qsTr("Shuffled")]
            currentIndex: root.shuffled ? 1 : 0
            onActivated: i => root.shuffled = i === 1
        }

        Label { text: qsTr("WHERE"); Layout.topMargin: 4 }
        SegmentedTabs {
            model: [qsTr("At the end of the queue"), qsTr("Play next")]
            currentIndex: root.next ? 1 : 0
            onActivated: i => root.next = i === 1
        }

        Text {
            Layout.fillWidth: true
            visible: root.count > root.limit
            text: qsTr("A room's queue holds up to %1 tracks — the first %1 go in.").arg(root.limit)
            color: Theme.textFaint
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 6
            spacing: 10
            Item { Layout.fillWidth: true }
            PillButton {
                kind: "ghost"
                text: qsTr("Cancel")
                fontSize: 13
                implicitHeight: 38
                onClicked: root.close()
            }
            PillButton {
                text: qsTr("Add %n tracks", "", root.count)
                fontSize: 13
                implicitHeight: 38
                enabled: root.count > 0
                onClicked: root.add()
            }
        }
    }
}
