import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import Nokturn

// Step 4: where the music comes from — folders on disk and a YouTube Music account.
Item {
    id: root

    ColumnLayout {
        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
        spacing: 24

        StepTitle {
            title: qsTr("What are we playing from?")
            subtitle: qsTr("Add music folders and connect your YouTube Music account. Both sources are optional — you can do it later on the Sources page.")
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 18

            // ---- local files ----
            StepCard {
                Layout.preferredWidth: 100
                icon: "folder"
                title: qsTr("Files on disk")
                description: qsTr("MP3, FLAC, M4A, OGG, Opus, WAV and more — with subfolders, tags and covers.")

                Text {
                    Layout.fillWidth: true
                    visible: Library.folders.length === 0
                    text: qsTr("No folder chosen yet.")
                    color: Theme.textFaint
                    font.pixelSize: 13
                }

                Repeater {
                    model: Library.folders
                    Rectangle {
                        id: folderRow
                        required property var modelData
                        Layout.fillWidth: true
                        implicitHeight: 48
                        radius: 12
                        color: Theme.alpha(Theme.text, 0.045)
                        RowLayout {
                            anchors { fill: parent; leftMargin: 14; rightMargin: 6 }
                            spacing: 10
                            Icon { name: "folder"; size: 18; color: "#9fbfa8" }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 1
                                Text {
                                    Layout.fillWidth: true
                                    text: folderRow.modelData.path
                                    color: Theme.text
                                    font.pixelSize: 13
                                    elide: Text.ElideMiddle
                                }
                                Text {
                                    text: qsTr("%n tracks", "", folderRow.modelData.count)
                                    color: Theme.textFaint
                                    font.pixelSize: 11
                                }
                            }
                            IconButton {
                                icon: "close"
                                iconSize: 16
                                showDot: false
                                onClicked: Library.removeFolder(folderRow.modelData.path)
                            }
                        }
                    }
                }

                // scan progress
                ColumnLayout {
                    Layout.fillWidth: true
                    visible: Library.scanning
                    spacing: 6
                    Text {
                        Layout.fillWidth: true
                        text: Library.scanStatus
                        color: Theme.textDim
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        height: 5
                        radius: 2.5
                        color: Theme.alpha(Theme.text, 0.08)
                        Rectangle {
                            width: parent.width * Library.scanProgress
                            height: parent.height
                            radius: 2.5
                            color: Theme.accent
                            Behavior on width { NumberAnimation { duration: 300 } }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    PillButton {
                        kind: Library.folders.length === 0 ? "primary" : "secondary"
                        icon: "add"
                        text: qsTr("Add folder")
                        onClicked: Nav.addFolder()
                    }
                    Text {
                        Layout.fillWidth: true
                        visible: Library.localCount > 0
                        text: qsTr("In the library: %n tracks", "", Library.localCount)
                        color: Theme.textDim
                        font.pixelSize: 13
                        horizontalAlignment: Text.AlignRight
                    }
                }
            }

            // ---- YouTube Music ----
            StepCard {
                Layout.preferredWidth: 100
                icon: "playCircle"
                title: "YouTube Music"
                description: qsTr("Liked songs, playlists from your library and search. Search and playback also work without signing in.")

                // Connected account
                RowLayout {
                    Layout.fillWidth: true
                    visible: YouTube.connected
                    spacing: 12
                    Item {
                        width: 44; height: 44
                        Image {
                            id: avatar
                            anchors.fill: parent
                            source: YouTube.accountPhoto
                            fillMode: Image.PreserveAspectCrop
                            visible: false
                        }
                        Rectangle { id: avatarMask; anchors.fill: parent; radius: 22; visible: false; layer.enabled: true }
                        MultiEffect {
                            anchors.fill: parent
                            source: avatar
                            maskEnabled: true
                            maskSource: avatarMask
                            visible: avatar.status === Image.Ready
                        }
                        Rectangle {
                            anchors.fill: parent
                            radius: 22
                            visible: avatar.status !== Image.Ready
                            color: Theme.alpha(Theme.accent, 0.25)
                            Icon { anchors.centerIn: parent; name: "person"; size: 22; color: Theme.accent }
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1
                        Text {
                            Layout.fillWidth: true
                            text: YouTube.accountName || qsTr("Google account")
                            color: Theme.text
                            font.pixelSize: 15
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }
                        Text {
                            Layout.fillWidth: true
                            text: YouTube.syncing ? qsTr("Syncing with your account…")
                                : qsTr("%n liked tracks", "", YouTube.likedCount)
                                  + " · " + qsTr("%n playlists", "", YouTube.playlistCount)
                            color: Theme.textDim
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }
                    }
                    Rectangle {
                        width: connectedText.implicitWidth + 20
                        height: 26
                        radius: 13
                        color: Theme.alpha(Theme.success, 0.16)
                        Text {
                            id: connectedText
                            anchors.centerIn: parent
                            text: qsTr("Connected")
                            color: Theme.success
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                        }
                    }
                }

                // Signing in
                ColumnLayout {
                    Layout.fillWidth: true
                    visible: !YouTube.connected
                    spacing: 10
                    Text {
                        Layout.fillWidth: true
                        text: YouTube.loggingIn ? YouTube.loginStatus
                            : YouTube.browserName !== ""
                              ? qsTr("You'll sign in in a separate %1 window with a clean profile — your regular browser profiles are not touched.").arg(YouTube.browserName)
                              : qsTr("Chrome, Edge or Brave was not found — you can connect the account later on the Sources page by pasting cookies.")
                        color: YouTube.loggingIn ? Theme.text : Theme.textDim
                        font.pixelSize: 13
                        wrapMode: Text.WordWrap
                    }
                    RowLayout {
                        spacing: 10
                        PillButton {
                            kind: "primary"
                            icon: "person"
                            text: YouTube.loggingIn ? qsTr("Waiting for sign-in…") : qsTr("Connect account")
                            busy: YouTube.loggingIn
                            enabled: YouTube.browserName !== "" && !YouTube.loggingIn
                            onClicked: YouTube.login()
                        }
                        PillButton {
                            visible: YouTube.loggingIn
                            kind: "ghost"
                            text: qsTr("Cancel")
                            onClicked: YouTube.cancelLogin()
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: Qt.rgba(1, 1, 1, 0.06)
                }

                // Playback engine (yt-dlp)
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Icon {
                        name: YouTube.engineReady ? "check" : "playCircle"
                        size: 20
                        color: YouTube.engineReady ? Theme.success : Theme.textDim
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Playback engine (yt-dlp)")
                            color: Theme.text
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                        }
                        Text {
                            Layout.fillWidth: true
                            text: YouTube.engineDownloading ? qsTr("Downloading… %1%").arg(Math.round(YouTube.engineProgress * 100))
                                : YouTube.engineReady ? qsTr("Ready") + (YouTube.engineVersion ? " · " + qsTr("version %1").arg(YouTube.engineVersion) : "")
                                : qsTr("Needed to play from YouTube — the official release from GitHub, verified with SHA-256.")
                            color: Theme.textDim
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }
                    }
                    PillButton {
                        visible: !YouTube.engineReady
                        kind: "secondary"
                        icon: "add"
                        text: qsTr("Download")
                        busy: YouTube.engineDownloading
                        enabled: !YouTube.engineDownloading
                        onClicked: YouTube.downloadEngine()
                    }
                }
            }
        }
    }
}
