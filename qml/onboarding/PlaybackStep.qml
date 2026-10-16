import QtQuick
import QtQuick.Layouts
import Nokturn

// Step 5: transitions between tracks (crossfade / mix style) and lyrics.
Item {
    id: root

    ColumnLayout {
        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
        spacing: 24

        StepTitle {
            title: qsTr("How should it play?")
            subtitle: qsTr("Set up transitions between tracks and lyrics. You can also turn on Mix for each playlist separately — with the “Mix” button on its page.")
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 18

            StepCard {
                Layout.preferredWidth: 130
                icon: "mix"
                title: qsTr("Transitions between tracks")
                description: Player.crossfade > 0
                    ? qsTr("The end of a track overlaps the start of the next one — %1.").arg(Player.mixStyleName(Player.crossfadeStyle).toLowerCase())
                    : qsTr("No overlap — tracks play one after another.")

                CrossfadePreview {
                    Layout.fillWidth: true
                    seconds: Player.crossfade
                    style: Player.crossfadeStyle
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14
                    Text {
                        text: qsTr("Length")
                        color: Theme.textDim
                        font.pixelSize: 13
                    }
                    ProgressSlider {
                        Layout.fillWidth: true
                        value: Player.crossfade / 12
                        labelFor: function(v) { return Math.round(v * 12) === 0 ? qsTr("off") : qsTr("%1 s").arg(Math.round(v * 12)) }
                        onMoved: v => Player.crossfade = Math.round(v * 12)
                    }
                    Text {
                        Layout.preferredWidth: 36
                        text: Player.crossfade === 0 ? qsTr("off") : qsTr("%1 s").arg(Player.crossfade)
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
                        Text { text: qsTr("Smart transitions"); color: Theme.text; font.pixelSize: 14; font.weight: Font.DemiBold }
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Skips silence at the end and start of tracks and detects fade-outs in the recording.")
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
                title: qsTr("Lyrics")
                description: qsTr("Fetched automatically from YouTube Music (synced when available) or from .lrc files next to the tracks. You'll find them in the “Now playing” panel and in full-screen mode.")

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        RowLayout {
                            spacing: 8
                            Text { text: qsTr("Automatic timing"); color: Theme.text; font.pixelSize: 14; font.weight: Font.DemiBold }
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
                                ? qsTr("Lyrics without timestamps get them from singing recognition (whisper.cpp, locally). Your hardware: %1 — recommended model: %2.").arg(Lyrics.hardwareSummary).arg(Lyrics.recommendedModel)
                                : qsTr("Requires stronger hardware (at least 4 CPU threads and 8 GB of RAM). Yours: %1.").arg(Lyrics.hardwareSummary)
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
                    text: qsTr("You'll download the required components (the whisper.cpp program and the %1 model, %2) in Settings → Lyrics.").arg(Lyrics.sttModel).arg(Lyrics.modelSize(Lyrics.sttModel))
                    color: Theme.warning
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
}
