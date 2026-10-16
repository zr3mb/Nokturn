import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import Nokturn

// Krok 4: skąd brać muzykę — foldery na dysku i konto YouTube Music.
Item {
    id: root

    ColumnLayout {
        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
        spacing: 24

        StepTitle {
            title: "Skąd gramy?"
            subtitle: "Dodaj foldery z muzyką i połącz konto YouTube Music. Oba źródła są opcjonalne — możesz to zrobić później w zakładce Źródła."
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 18

            // ---- pliki lokalne ----
            StepCard {
                Layout.preferredWidth: 100
                icon: "folder"
                title: "Pliki na dysku"
                description: "MP3, FLAC, M4A, OGG, Opus, WAV i inne — razem z podfolderami, tagami i okładkami."

                Text {
                    Layout.fillWidth: true
                    visible: Library.folders.length === 0
                    text: "Nie wybrano jeszcze żadnego folderu."
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
                                    text: Theme.plural(folderRow.modelData.count, "utwór", "utwory", "utworów")
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

                // postęp skanowania
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
                        text: "Dodaj folder"
                        onClicked: Nav.addFolder()
                    }
                    Text {
                        Layout.fillWidth: true
                        visible: Library.localCount > 0
                        text: "W bibliotece: " + Theme.plural(Library.localCount, "utwór", "utwory", "utworów")
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
                description: "Polubione utwory, playlisty z biblioteki i wyszukiwarka. Wyszukiwanie i odtwarzanie działają też bez logowania."

                // Połączone konto
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
                            text: YouTube.accountName || "Konto Google"
                            color: Theme.text
                            font.pixelSize: 15
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }
                        Text {
                            Layout.fillWidth: true
                            text: YouTube.syncing ? "Synchronizacja z kontem…"
                                : Theme.plural(YouTube.likedCount, "polubiony utwór", "polubione utwory", "polubionych utworów")
                                  + " · " + Theme.plural(YouTube.playlistCount, "playlista", "playlisty", "playlist")
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
                            text: "Połączono"
                            color: Theme.success
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                        }
                    }
                }

                // Logowanie
                ColumnLayout {
                    Layout.fillWidth: true
                    visible: !YouTube.connected
                    spacing: 10
                    Text {
                        Layout.fillWidth: true
                        text: YouTube.loggingIn ? YouTube.loginStatus
                            : YouTube.browserName !== ""
                              ? "Zalogujesz się w osobnym oknie " + YouTube.browserName + " z czystym profilem — Twoje zwykłe profile przeglądarki nie są dotykane."
                              : "Nie znaleziono Chrome, Edge ani Brave — konto połączysz później w Źródłach, wklejając ciasteczka."
                        color: YouTube.loggingIn ? Theme.text : Theme.textDim
                        font.pixelSize: 13
                        wrapMode: Text.WordWrap
                    }
                    RowLayout {
                        spacing: 10
                        PillButton {
                            kind: "primary"
                            icon: "person"
                            text: YouTube.loggingIn ? "Czekam na logowanie…" : "Połącz konto"
                            busy: YouTube.loggingIn
                            enabled: YouTube.browserName !== "" && !YouTube.loggingIn
                            onClicked: YouTube.login()
                        }
                        PillButton {
                            visible: YouTube.loggingIn
                            kind: "ghost"
                            text: "Anuluj"
                            onClicked: YouTube.cancelLogin()
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: Qt.rgba(1, 1, 1, 0.06)
                }

                // Silnik odtwarzania (yt-dlp)
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
                            text: "Silnik odtwarzania (yt-dlp)"
                            color: Theme.text
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                        }
                        Text {
                            Layout.fillWidth: true
                            text: YouTube.engineDownloading ? "Pobieranie… " + Math.round(YouTube.engineProgress * 100) + "%"
                                : YouTube.engineReady ? "Gotowy" + (YouTube.engineVersion ? " · wersja " + YouTube.engineVersion : "")
                                : "Potrzebny do odtwarzania z YouTube — oficjalne wydanie z GitHuba, sprawdzane sumą SHA-256."
                            color: Theme.textDim
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }
                    }
                    PillButton {
                        visible: !YouTube.engineReady
                        kind: "secondary"
                        icon: "add"
                        text: "Pobierz"
                        busy: YouTube.engineDownloading
                        enabled: !YouTube.engineDownloading
                        onClicked: YouTube.downloadEngine()
                    }
                }
            }
        }
    }
}
