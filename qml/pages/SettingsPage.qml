import QtQuick
import QtQuick.Layouts
import Nokturn

Item {
    id: page
    property var args: ({})
    property real crossfade: 0.33
    property int quality: 2

    component Heading: Text {
        Layout.topMargin: 6
        Layout.bottomMargin: 4
        color: Theme.text
        font.family: Theme.fontDisplay
        font.pixelSize: 20
        font.weight: Font.DemiBold
    }

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

    PageFlick {
        anchors.fill: parent
        spacing: 26

        Reveal {
            Text {
                text: "Ustawienia"
                color: Theme.text
                font.family: Theme.fontDisplay
                font.pixelSize: 38
                font.weight: Font.Bold
            }
        }

        Reveal {
            delay: 60
            ColumnLayout {
                width: parent.width
                spacing: 8
                Heading { text: "Wygląd" }
                SettingRow {
                    icon: "sparkle"
                    title: "Dynamiczny kolor akcentu"
                    description: "Kolory interfejsu płynnie dopasowują się do okładki odtwarzanego utworu."
                    Toggle { checked: Theme.dynamicAccent; onToggled: c => Theme.dynamicAccent = c }
                }
                SettingRow {
                    icon: "album"
                    title: "Żywe tło"
                    description: "Miękkie światło za interfejsem, delikatnie pulsujące w rytm muzyki."
                    Toggle { checked: Theme.ambient; onToggled: c => Theme.ambient = c }
                }
                SettingRow {
                    icon: "menuOpen"
                    title: "Kompaktowy panel boczny"
                    description: "Pokazuj w panelu bocznym tylko ikony."
                    Toggle { checked: Nav.sidebarCompact; onToggled: c => Nav.sidebarCompact = c }
                }
            }
        }

        Reveal {
            delay: 120
            ColumnLayout {
                width: parent.width
                spacing: 8
                Heading { text: "Odtwarzanie" }
                SettingRow {
                    icon: "tune"
                    title: "Płynne przejścia (crossfade)"
                    description: "Nakładanie końca utworu na początek następnego."
                    Row {
                        spacing: 14
                        ProgressSlider {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 170
                            value: page.crossfade
                            labelFor: function(v) { return Math.round(v * 12) + " s" }
                            onDragging: v => page.crossfade = v
                            onMoved: v => page.crossfade = v
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 34
                            text: Math.round(page.crossfade * 12) + " s"
                            color: Theme.textDim
                            font.pixelSize: 13
                            font.features: { "tnum": 1 }
                        }
                    }
                }
                SettingRow {
                    icon: "volumeUp"
                    title: "Normalizacja głośności"
                    description: "Wyrównuje głośność utworów z różnych źródeł."
                    Toggle { checked: true }
                }
                SettingRow {
                    icon: "queue"
                    title: "Odtwarzanie bez przerw"
                    description: "Albumy koncertowe i miksy grają bez ciszy między utworami."
                    Toggle { checked: false }
                }
            }
        }

        Reveal {
            delay: 180
            ColumnLayout {
                width: parent.width
                spacing: 8
                Heading { text: "Jakość strumienia" }
                SettingRow {
                    icon: "playCircle"
                    title: "YouTube Music i radio"
                    description: ["Najmniejsze zużycie danych.", "Zrównoważona jakość.", "Najwyższa dostępna jakość (do 256 kbps).", "Bezstratnie, gdy źródło na to pozwala."][page.quality]
                    SegmentedTabs {
                        model: ["Oszczędna", "Normalna", "Wysoka", "Bezstratna"]
                        currentIndex: page.quality
                        onActivated: i => page.quality = i
                    }
                }
            }
        }

        Reveal {
            delay: 240
            ColumnLayout {
                width: parent.width
                spacing: 12
                Heading { text: "Skróty klawiszowe" }
                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 30
                    rowSpacing: 12

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
                            required property var modelData
                            Layout.fillWidth: true
                            spacing: 6
                            Repeater {
                                model: parent.modelData.keys
                                Key {
                                    required property var modelData
                                    label: modelData
                                }
                            }
                            Text {
                                Layout.leftMargin: 8
                                Layout.fillWidth: true
                                text: parent.modelData.label
                                color: Theme.textDim
                                font.pixelSize: 13
                            }
                        }
                    }
                }
            }
        }

        Reveal {
            delay: 300
            RowLayout {
                width: parent.width
                spacing: 14
                LogoMark {}
                ColumnLayout {
                    spacing: 2
                    Text { text: "Nokturn 0.1 — podgląd interfejsu"; color: Theme.text; font.pixelSize: 14; font.weight: Font.DemiBold }
                    Text { text: "C++20 · Qt 6 · Qt Quick"; color: Theme.textFaint; font.pixelSize: 12 }
                }
            }
        }
    }
}
