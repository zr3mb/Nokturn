import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nokturn

// Playlist folders: a new one (optionally with a playlist moved into it), renaming or deleting one, and
// picking the folder for a playlist (Library's folders; kept on the Nokturn account).
Popup {
    id: root
    property string mode: "create"     // "create" | "edit" | "pick"
    property string folderId: ""
    property int playlistIndex: -1      // create (moved into it) / pick: the playlist (Library.playlists)
    readonly property var playlist: playlistIndex >= 0 ? Library.playlists[playlistIndex] || ({}) : ({})

    function create(index) {
        mode = "create"
        playlistIndex = index
        field.text = ""
        open()
        field.forceActiveFocus()
    }

    function edit(id) {
        mode = "edit"
        folderId = id
        const f = Library.playlistFolders.find(x => x.id === id)
        field.text = f ? f.name : ""
        open()
        field.forceActiveFocus()
        field.selectAll()
    }

    function pick(index) {
        mode = "pick"
        playlistIndex = index
        open()
    }

    function save() {
        const name = field.text.trim()
        if (!name)
            return
        if (mode === "create")
            Library.createPlaylistFolder(name, playlistIndex)
        else
            Library.renamePlaylistFolder(folderId, name)
        close()
    }

    anchors.centerIn: Overlay.overlay
    width: Math.min(440, (Overlay.overlay ? Overlay.overlay.width : 480) - 40)
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

    contentItem: ColumnLayout {
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            Icon { name: "folder"; size: 22; color: Theme.accent }
            Text {
                Layout.fillWidth: true
                text: root.mode === "pick" ? qsTr("Move to a folder") : root.mode === "edit" ? qsTr("Folder") : qsTr("New folder")
                color: Theme.text
                font.pixelSize: 18
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
        }
        Text {
            Layout.fillWidth: true
            visible: root.mode === "pick" || (root.mode === "create" && root.playlistIndex >= 0)
            text: root.playlist.name || ""
            color: Theme.textDim
            font.pixelSize: 13
            elide: Text.ElideRight
        }

        // picking: the folders, none, or a new one
        ColumnLayout {
            Layout.fillWidth: true
            visible: root.mode === "pick"
            spacing: 2
            MenuEntry {
                Layout.fillWidth: true
                icon: "close"
                text: qsTr("No folder (with its source)")
                onTriggered: { Library.movePlaylistToFolder(root.playlistIndex, ""); root.close() }
            }
            Repeater {
                model: root.mode === "pick" ? Library.playlistFolders : []
                MenuEntry {
                    required property var modelData
                    Layout.fillWidth: true
                    icon: root.playlist.folder === modelData.id ? "check" : "folder"
                    text: modelData.name
                    onTriggered: { Library.movePlaylistToFolder(root.playlistIndex, modelData.id); root.close() }
                }
            }
            MenuEntry {
                Layout.fillWidth: true
                icon: "add"
                text: qsTr("New folder…")
                onTriggered: root.create(root.playlistIndex)
            }
        }

        // a name
        InputField {
            id: field
            Layout.fillWidth: true
            visible: root.mode !== "pick"
            icon: "folder"
            maximumLength: 60
            placeholderText: qsTr("Folder name")
            onAccepted: root.save()
        }
        RowLayout {
            Layout.fillWidth: true
            visible: root.mode !== "pick"
            spacing: 10
            PillButton {
                visible: root.mode === "edit"
                kind: "ghost"
                text: qsTr("Delete the folder")
                fontSize: 13
                implicitHeight: 38
                onClicked: { Library.deletePlaylistFolder(root.folderId); root.close() }
            }
            Item { Layout.fillWidth: true }
            PillButton {
                kind: "ghost"
                text: qsTr("Cancel")
                fontSize: 13
                implicitHeight: 38
                onClicked: root.close()
            }
            PillButton {
                text: root.mode === "edit" ? qsTr("Save") : qsTr("Create")
                fontSize: 13
                implicitHeight: 38
                enabled: field.text.trim() !== ""
                onClicked: root.save()
            }
        }
    }
}
