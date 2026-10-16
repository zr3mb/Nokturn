import QtQuick
import QtQuick.Layouts
import Nokturn

// Krok 6: Discord i sterowanie (skróty, klawisze multimedialne).
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
            title: "Integracje"
            subtitle: "Pokaż znajomym, czego słuchasz, i steruj muzyką z klawiatury."
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 18

            StepCard {
                Layout.preferredWidth: 100
                icon: "chat"
                title: "Status na Discordzie"
                description: "„Słucha” z tytułem, wykonawcą, okładką i paskiem postępu. Działa z aplikacją Discord uruchomioną na tym komputerze."

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Text {
                        Layout.fillWidth: true
                        text: "Pokazuj, czego słuchasz"
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
                            ? "Używany jest wbudowany identyfikator aplikacji."
                            : "Discord wymaga identyfikatora aplikacji: utwórz ją na discord.com/developers/applications (jej nazwa pojawi się w statusie), skopiuj „Application ID” i wklej poniżej."
                        color: Theme.textDim
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                    }
                    InputField {
                        Layout.fillWidth: true
                        implicitHeight: 40
                        icon: "key"
                        placeholderText: "Application ID, np. 1234567890123456789"
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
                title: "Sterowanie"
                description: root.linux
                    ? "Klawisze multimedialne, kontrolki w panelu GNOME/KDE i playerctl działają od razu (MPRIS)."
                    : "Najważniejsze skróty klawiszowe — pełna lista jest w Ustawieniach."

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 18
                    rowSpacing: 10
                    Repeater {
                        model: [
                            { keys: ["Spacja"], label: "Odtwórz / pauza" },
                            { keys: ["Ctrl", "→"], label: "Następny utwór" },
                            { keys: ["Ctrl", "←"], label: "Poprzedni utwór" },
                            { keys: ["Ctrl", "K"], label: "Szukaj" },
                            { keys: ["F11"], label: "Tryb pełnoekranowy" },
                            { keys: ["Alt", "←"], label: "Wstecz" }
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
