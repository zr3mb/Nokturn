import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nokturn

// A shared playlist's password: asked when joining one that has it, or set / removed by its owner.
Popup {
    id: root
    property string playlistId: ""
    property bool setting: false       // the owner sets it (otherwise: joining)
    property bool wrong: false         // the last one entered didn't match
    property bool protectedNow: false  // the playlist has a password already (setting)

    function ask(id, wasWrong) {
        playlistId = id
        setting = false
        wrong = wasWrong
        field.text = ""
        open()
        field.forceActiveFocus()
    }

    function edit(id) {
        playlistId = id
        setting = true
        wrong = false
        protectedNow = Online.playlistInfo(id).protected === true
        field.text = ""
        open()
        field.forceActiveFocus()
    }

    function submit() {
        if (field.text === "" && !root.setting)
            return
        if (root.setting)
            Online.setPlaylistPassword(root.playlistId, field.text)
        else
            Online.joinWithPassword(root.playlistId, field.text)
        close()
    }

    anchors.centerIn: Overlay.overlay
    width: Math.min(460, (Overlay.overlay ? Overlay.overlay.width : 500) - 40)
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
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            Icon { name: "key"; size: 22; color: Theme.accent }
            Text {
                Layout.fillWidth: true
                text: root.setting ? qsTr("Playlist password") : qsTr("This playlist has a password")
                color: Theme.text
                font.pixelSize: 18
                font.weight: Font.DemiBold
                wrapMode: Text.WordWrap
            }
        }
        Text {
            Layout.fillWidth: true
            text: root.setting
                  ? (root.protectedNow ? qsTr("Others need it to join through the link; those already in stay. Leave it empty and save to remove it.")
                                       : qsTr("Others will need it to join through the link; those already in stay."))
                  : root.wrong ? qsTr("Wrong password — try again.")
                               : qsTr("Ask the person who shared it.")
            color: root.wrong ? Theme.danger : Theme.textDim
            font.pixelSize: 13
            wrapMode: Text.WordWrap
        }
        InputField {
            id: field
            Layout.fillWidth: true
            icon: "key"
            echoMode: TextInput.Password
            maximumLength: 100
            placeholderText: qsTr("Password")
            onAccepted: root.submit()
        }
        RowLayout {
            Layout.fillWidth: true
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
                text: root.setting ? (field.text === "" && root.protectedNow ? qsTr("Remove") : qsTr("Save")) : qsTr("Join")
                fontSize: 13
                implicitHeight: 38
                enabled: field.text !== "" || (root.setting && root.protectedNow)
                onClicked: root.submit()
            }
        }
    }
}
