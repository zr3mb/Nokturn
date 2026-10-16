import QtQuick
import QtQuick.Layouts
import Nokturn

// Step 6: Discord and controls (shortcuts, media keys).
Item {
    id: root
    readonly property bool linux: Qt.platform.os === "linux"

    component Key: Rectangle {
        property string label: ""
        implicitWidth: keyText.implicitWidth + 16
        implicitHeight: 26
        radius: 7
        color: Theme.alpha(Theme.text, 0.07)
        border.width: 1
        border.color: Theme.alpha(Theme.text, 0.1)
        Text {
            id: keyText
            anchors.centerIn: parent
            text: parent.label
            color: Theme.text
            font.pixelSize: 12
            font.weight: Font.DemiBold
        }
    }

    ColumnLayout {
        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
        spacing: 24

        StepTitle {
            title: qsTr("Integrations")
            subtitle: qsTr("Show your friends what you're listening to and control the music from the keyboard.")
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 18

            StepCard {
                Layout.preferredWidth: 100
                icon: "chat"
                title: qsTr("Discord status")
                description: qsTr("“Listening” with the title, artist, cover and a progress bar. Works with the Discord app running on this computer.")

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Show what you're listening to")
                        color: Theme.text
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                    }
                    Toggle { checked: Discord.enabled; onToggled: c => Discord.enabled = c }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    visible: Discord.enabled
                    spacing: 8
                    Text {
                        Layout.fillWidth: true
                        text: Discord.builtInClientId && Discord.clientId !== ""
                            ? qsTr("The built-in application ID is used.")
                            : qsTr("Discord requires an application ID: create one at discord.com/developers/applications (its name will show in the status), copy the “Application ID” and paste it below.")
                        color: Theme.textDim
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                    }
                    InputField {
                        Layout.fillWidth: true
                        implicitHeight: 40
                        icon: "key"
                        placeholderText: qsTr("Application ID, e.g. 1234567890123456789")
                        text: Discord.clientId
                        inputMethodHints: Qt.ImhDigitsOnly
                        onEditingFinished: Discord.clientId = text
                    }
                    RowLayout {
                        spacing: 8
                        Rectangle {
                            width: 8; height: 8; radius: 4
                            color: Discord.connected ? Theme.success : Theme.textFaint
                        }
                        Text {
                            text: Discord.status
                            color: Theme.textDim
                            font.pixelSize: 12
                        }
                    }
                }
            }

            StepCard {
                Layout.preferredWidth: 100
                icon: "tune"
                title: qsTr("Controls")
                description: root.linux
                    ? qsTr("Media keys, GNOME/KDE panel controls and playerctl work right away (MPRIS).")
                    : qsTr("The most important keyboard shortcuts — the full list is in Settings.")

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 18
                    rowSpacing: 10
                    Repeater {
                        model: [
                            { keys: [qsTr("Space")], label: qsTr("Play / pause") },
                            { keys: ["Ctrl", "→"], label: qsTr("Next track") },
                            { keys: ["Ctrl", "←"], label: qsTr("Previous track") },
                            { keys: ["Ctrl", "K"], label: qsTr("Search") },
                            { keys: ["F11"], label: qsTr("Full-screen mode") },
                            { keys: ["Alt", "←"], label: qsTr("Back") }
                        ]
                        RowLayout {
                            id: shortcut
                            required property var modelData
                            Layout.fillWidth: true
                            spacing: 6
                            Repeater {
                                model: shortcut.modelData.keys
                                Key {
                                    required property var modelData
                                    label: modelData
                                }
                            }
                            Text {
                                Layout.leftMargin: 6
                                Layout.fillWidth: true
                                text: shortcut.modelData.label
                                color: Theme.textDim
                                font.pixelSize: 13
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }
        }
    }
}
