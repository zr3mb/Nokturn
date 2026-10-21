import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nokturn

// New playlist — kept on this computer or on the YouTube Music account — or editing an existing one
// (name, description; who can see it for YouTube Music).
Popup {
    id: root
    property string mode: "create"      // "create" | "edit"
    property int index: -1              // edit: the playlist (Library.playlists)
    property var rows: []               // create: tracks to start with
    property bool youtube: false        // create: on YouTube Music
    property int privacy: 0             // 0 private, 1 unlisted, 2 public
    readonly property var privacyKeys: ["PRIVATE", "UNLISTED", "PUBLIC"]
    readonly property var playlist: index >= 0 ? Library.playlists[index] || ({}) : ({})
    readonly property bool editingYouTube: mode === "edit" && playlist.kind === "ytplaylist"

    function show(m, i, r) {
        mode = m
        index = i
        rows = r || []
        youtube = Prefs.boolValue("playlists/youtube", false) && YouTube.connected
        privacy = 0
        nameField.text = m === "edit" ? playlist.name || "" : ""
        descriptionField.text = m === "edit" ? playlist.description || "" : ""
        open()
        nameField.forceActiveFocus()
    }

    function accept() {
        if (mode === "create") {
            Prefs.setValue("playlists/youtube", youtube)
            Playlists.createPlaylist(nameField.text, descriptionField.text, youtube ? "youtube" : "local",
                                     privacyKeys[privacy], rows)
        } else {
            Playlists.edit(index, nameField.text, descriptionField.text)
        }
        close()
    }

    anchors.centerIn: Overlay.overlay
    width: Math.min(520, (Overlay.overlay ? Overlay.overlay.width : 600) - 60)
    modal: true
    focus: true
    padding: 26
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
        spacing: 16

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            Icon { name: root.mode === "create" ? "add" : "tune"; size: 22; color: Theme.accent }
            Text {
                Layout.fillWidth: true
                text: root.mode === "create" ? qsTr("New playlist") : qsTr("Edit playlist")
                color: Theme.text
                font.family: Theme.fontDisplay
                font.pixelSize: 20
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            IconButton { icon: "close"; showDot: false; onClicked: root.close() }
        }

        Text {
            visible: root.mode === "create" && root.rows.length > 0
            Layout.fillWidth: true
            text: qsTr("With %n tracks", "", root.rows.length)
            color: Theme.textDim
            font.pixelSize: 13
        }

        InputField {
            id: nameField
            Layout.fillWidth: true
            icon: "music"
            placeholderText: qsTr("Name")
            onAccepted: root.accept()
        }
        InputField {
            id: descriptionField
            Layout.fillWidth: true
            visible: !root.editingYouTube
            icon: "lyrics"
            placeholderText: qsTr("Description (optional)")
            onAccepted: root.accept()
        }

        // Where the playlist lives
        ColumnLayout {
            visible: root.mode === "create"
            Layout.fillWidth: true
            spacing: 8
            Text {
                text: qsTr("Save to")
                color: Theme.textDim
                font.pixelSize: 12
                font.weight: Font.DemiBold
            }
            SegmentedTabs {
                model: [qsTr("This computer"), "YouTube Music"]
                currentIndex: root.youtube ? 1 : 0
                onActivated: i => {
                    if (i === 1 && !YouTube.connected) {
                        currentIndex = 0
                        Nav.toast(qsTr("Sign in to YouTube Music first (Sources)"), "person")
                        return
                    }
                    root.youtube = i === 1
                }
            }
            Text {
                Layout.fillWidth: true
                text: root.youtube
                      ? qsTr("Saved on your YouTube Music account — you'll see it on your phone too. Spotify tracks go in as their YouTube Music version, local files are skipped.")
                      : qsTr("Saved only in Nokturn on this computer. It can hold any track — local files, YouTube Music, Spotify and SoundCloud.")
                color: Theme.textFaint
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
        }

        // Who can see it (YouTube Music)
        ColumnLayout {
            visible: root.mode === "create" && root.youtube
            Layout.fillWidth: true
            spacing: 8
            Text {
                text: qsTr("Who can see it")
                color: Theme.textDim
                font.pixelSize: 12
                font.weight: Font.DemiBold
            }
            SegmentedTabs {
                model: [qsTr("Private"), qsTr("Unlisted"), qsTr("Public")]
                currentIndex: root.privacy
                onActivated: i => root.privacy = i
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 6
            spacing: 10
            Item { Layout.fillWidth: true }
            PillButton {
                kind: "ghost"
                text: qsTr("Cancel")
                onClicked: root.close()
            }
            PillButton {
                icon: root.mode === "create" ? "add" : "check"
                text: root.mode === "create" ? qsTr("Create") : qsTr("Save")
                enabled: nameField.text.trim() !== ""
                onClicked: root.accept()
            }
        }
    }
}
