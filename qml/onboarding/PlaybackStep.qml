import QtQuick
import QtQuick.Layouts
import Nokturn

// Krok 5: przejścia między utworami (crossfade / styl miksu) i teksty utworów.
Item {
    id: root

    ColumnLayout {
        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
        spacing: 24

        StepTitle {
            title: "Jak ma grać?"
            subtitle: "Ustaw przejścia między utworami i teksty. Miks możesz też włączyć osobno dla każdej playlisty — przyciskiem „Miks” na jej stronie."
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 18

            StepCard {
                Layout.preferredWidth: 130
                icon: "mix"
                title: "Przejścia między utworami"
                description: Player.crossfade > 0
                    ? "Koniec utworu nakłada się na początek następnego — " + Player.mixStyleName(Player.crossfadeStyle).toLowerCase() + "."
                    : "Bez nakładania — utwory grają jeden po drugim."

                CrossfadePreview {
                    Layout.fillWidth: true
                    seconds: Player.crossfade
                    style: Player.crossfadeStyle
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14
                    Text {
                        text: "Długość"
                        color: Theme.textDim
                        font.pixelSize: 13
                    }
                    ProgressSlider {
                        Layout.fillWidth: true
                        value: Player.crossfade / 12
                        labelFor: function(v) { return Math.round(v * 12) === 0 ? "wył." : Math.round(v * 12) + " s" }
                        onMoved: v => Player.crossfade = Math.round(v * 12)
                    }
                    Text {
                        Layout.preferredWidth: 36
                        text: Player.crossfade === 0 ? "wył." : Player.crossfade + " s"
                        color: Theme.text
                        font.pixelSize: 13
                        font.features: { "tnum": 1 }
                    }
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: 8
                    opacity: Player.crossfade > 0 ? 1 : 0.4
                    enabled: Player.crossfade > 0
                    Repeater {
                        model: Player.mixStyles
                        PillButton {
                            required property string modelData
                            kind: Player.crossfadeStyle === modelData ? "primary" : "secondary"
                            text: Player.mixStyleName(modelData)
                            fontSize: 12
                            implicitHeight: 32
                            onClicked: Player.crossfadeStyle = modelData
                        }
                    }
                }
                Text {
                    Layout.fillWidth: true
                    visible: Player.crossfade > 0
                    text: Player.mixStyleDescription(Player.crossfadeStyle)
                    color: Theme.textFaint
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text { text: "Inteligentne przejścia"; color: Theme.text; font.pixelSize: 14; font.weight: Font.DemiBold }
                        Text {
                            Layout.fillWidth: true
                            text: "Pomija ciszę na końcu i początku utworów oraz wykrywa wyciszenia w nagraniu."
                            color: Theme.textDim
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }
                    }
                    Toggle { checked: Player.smartTransitions; onToggled: c => Player.smartTransitions = c }
                }
            }

            StepCard {
                Layout.preferredWidth: 100
                icon: "lyrics"
                title: "Teksty utworów"
                description: "Pobierane automatycznie z YouTube Music (z synchronizacją, gdy jest dostępna) albo z plików .lrc obok utworów. Znajdziesz je w panelu „Teraz odtwarzane” i w trybie pełnoekranowym."

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        RowLayout {
                            spacing: 8
                            Text { text: "Dopasowanie czasu"; color: Theme.text; font.pixelSize: 14; font.weight: Font.DemiBold }
                            Rectangle {
                                width: alphaText.implicitWidth + 12
                                height: 18
                                radius: 9
                                color: Theme.alpha(Theme.warning, 0.18)
                                Text {
                                    id: alphaText
                                    anchors.centerIn: parent
                                    text: "ALPHA"
                                    color: Theme.warning
                                    font.pixelSize: 9
                                    font.weight: Font.Bold
                                    font.letterSpacing: 1
                                }
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            text: Lyrics.hardwareOk
                                ? "Teksty bez czasów dostaną je z rozpoznawania śpiewu (whisper.cpp, lokalnie). Twój sprzęt: " + Lyrics.hardwareSummary + " — zalecany model: " + Lyrics.recommendedModel + "."
                                : "Wymaga mocniejszego sprzętu (min. 4 wątki procesora i 8 GB RAM). Twój: " + Lyrics.hardwareSummary + "."
                            color: Theme.textDim
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }
                    }
                    Toggle {
                        enabled: Lyrics.hardwareOk
                        opacity: enabled ? 1 : 0.4
                        checked: Lyrics.autoAlign
                        onToggled: c => Lyrics.autoAlign = c
                    }
                }
                Text {
                    Layout.fillWidth: true
                    visible: Lyrics.autoAlign && !Lyrics.sttReady
                    text: "Potrzebne składniki (program whisper.cpp i model " + Lyrics.sttModel + ", " + Lyrics.modelSize(Lyrics.sttModel) + ") pobierzesz w Ustawieniach → Teksty utworów."
                    color: Theme.warning
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
}
