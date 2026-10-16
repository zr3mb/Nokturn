import QtQuick
import QtQuick.Layouts
import Nokturn

Item {
    id: page
    property var args: ({})
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
                text: qsTr("Settings")
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
                Heading { text: qsTr("Appearance") }
                SettingRow {
                    icon: "sparkle"
                    title: qsTr("First-run wizard")
                    description: qsTr("Go through choosing the layout, colors, music sources, transitions and integrations again.")
                    PillButton {
                        kind: "secondary"
                        icon: "refresh"
                        text: qsTr("Run the wizard")
                        fontSize: 13
                        implicitHeight: 36
                        onClicked: Nav.startOnboarding()
                    }
                }
                SettingRow {
                    icon: "chat"
                    title: qsTr("Language")
                    description: qsTr("Interface language. “System” follows the language of your operating system.")
                    SegmentedTabs {
                        model: [qsTr("System"), "English", "Polski"]
                        currentIndex: ["system", "en", "pl"].indexOf(Language.language)
                        onActivated: i => Language.language = ["system", "en", "pl"][i]
                    }
                }
                SettingRow {
                    icon: "sparkle"
                    title: qsTr("Dynamic accent color")
                    description: qsTr("Interface colors smoothly adapt to the cover of the playing track.")
                    Toggle { checked: Theme.dynamicAccent; onToggled: c => Theme.dynamicAccent = c }
                }
                SettingRow {
                    icon: "album"
                    title: qsTr("Live background")
                    description: qsTr("Soft light behind the interface, gently pulsing with the music.")
                    Toggle { checked: Theme.ambient; onToggled: c => Theme.ambient = c }
                }
                SettingRow {
                    icon: "menuOpen"
                    title: qsTr("Compact sidebar")
                    description: qsTr("Show only icons in the sidebar.")
                    Toggle { checked: Nav.sidebarCompact; onToggled: c => Nav.sidebarCompact = c }
                }
            }
        }

        Reveal {
            delay: 120
            ColumnLayout {
                width: parent.width
                spacing: 8
                Heading { text: qsTr("Playback") }
                SettingRow {
                    icon: "tune"
                    title: qsTr("Smooth transitions (crossfade)")
                    description: Player.crossfade > 0
                        ? qsTr("The end of a track overlaps the start of the next one. Playlists with “Mix” turned on use their own settings.")
                        : qsTr("Off — tracks play one after another. You can also turn on Mix for a single playlist.")
                    Row {
                        spacing: 14
                        ProgressSlider {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 170
                            value: Player.crossfade / 12
                            labelFor: function(v) { return Math.round(v * 12) === 0 ? qsTr("off") : qsTr("%1 s").arg(Math.round(v * 12)) }
                            onMoved: v => Player.crossfade = Math.round(v * 12)
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 34
                            text: Player.crossfade === 0 ? qsTr("off") : qsTr("%1 s").arg(Player.crossfade)
                            color: Theme.textDim
                            font.pixelSize: 13
                            font.features: { "tnum": 1 }
                        }
                    }
                }
                SettingRow {
                    visible: Player.crossfade > 0
                    icon: "sparkle"
                    title: qsTr("Transition style")
                    description: Player.mixStyleDescription(Player.crossfadeStyle)
                    SegmentedTabs {
                        model: Player.mixStyles.map(s => Player.mixStyleName(s))
                        currentIndex: Player.mixStyles.indexOf(Player.crossfadeStyle)
                        onActivated: i => Player.crossfadeStyle = Player.mixStyles[i]
                    }
                }
                SettingRow {
                    icon: "next"
                    title: qsTr("Smart transitions")
                    description: qsTr("The mix starts when the music actually ends: it skips silence at the end and start of tracks and detects fade-outs recorded in the track.")
                    Toggle { checked: Player.smartTransitions; onToggled: c => Player.smartTransitions = c }
                }
                SettingRow {
                    icon: "queue"
                    title: qsTr("Keep playing (autoplay)")
                    description: qsTr("When a playlist ends, the music continues with the next playlist; after an album or search results — with your library.")
                    Toggle { checked: Player.autoplay; onToggled: c => Player.autoplay = c }
                }
                SettingRow {
                    icon: "volumeUp"
                    title: qsTr("Volume normalization")
                    description: qsTr("Evens out the loudness of tracks from different sources.")
                    Toggle { checked: true }
                }
                SettingRow {
                    icon: "queue"
                    title: qsTr("Gapless playback")
                    description: qsTr("Live albums and mixes play without silence between tracks.")
                    Toggle { checked: false }
                }
            }
        }

        Reveal {
            delay: 150
            ColumnLayout {
                width: parent.width
                spacing: 8
                Heading { text: qsTr("Lyrics") }
                SettingRow {
                    icon: "lyrics"
                    title: qsTr("Lyrics from YouTube Music")
                    description: qsTr("Lyrics of the playing track (synced when available) in the “Now playing” panel and in full-screen mode. For local files the track is looked up by artist and title; as a fallback, an .lrc file next to the track or lyrics stored in the tags are used.")
                }
                SettingRow {
                    icon: "sparkle"
                    title: qsTr("Automatic timing (alpha)")
                    description: Lyrics.hardwareOk
                        ? qsTr("When lyrics have no timestamps, Nokturn recognizes the singing in the recording (whisper.cpp, locally on your computer) and aligns the lyric lines to it. It puts a heavy load on the CPU for tens of seconds to a few minutes per track; the result is remembered. Your hardware: %1.").arg(Lyrics.hardwareSummary)
                        : qsTr("Requires stronger hardware (at least 4 CPU threads and 8 GB of RAM). Your hardware: %1.").arg(Lyrics.hardwareSummary)
                    Toggle {
                        enabled: Lyrics.hardwareOk
                        opacity: enabled ? 1 : 0.4
                        checked: Lyrics.autoAlign
                        onToggled: c => Lyrics.autoAlign = c
                    }
                }
                SettingRow {
                    visible: Lyrics.autoAlign
                    icon: "tune"
                    title: qsTr("Speech recognition model")
                    description: qsTr("A larger model recognizes singing better but runs slower.") + " "
                        + Lyrics.sttModel + ": " + Lyrics.modelSize(Lyrics.sttModel)
                        + " · " + (Lyrics.sttModel === Lyrics.recommendedModel ? qsTr("recommended for your hardware") : qsTr("recommended: %1").arg(Lyrics.recommendedModel))
                    SegmentedTabs {
                        model: Lyrics.sttModels.map(m => m.charAt(0).toUpperCase() + m.slice(1))
                        currentIndex: Lyrics.sttModels.indexOf(Lyrics.sttModel)
                        onActivated: i => Lyrics.sttModel = Lyrics.sttModels[i]
                    }
                }
                SettingRow {
                    visible: Lyrics.autoAlign
                    icon: Lyrics.sttReady ? "check" : "add"
                    title: Lyrics.sttReady ? qsTr("Speech recognition is ready") : qsTr("Download components")
                    description: Lyrics.sttDownloading
                        ? qsTr("Downloading: %1… %2%").arg(Lyrics.sttDownloadLabel).arg(Math.round(Lyrics.sttProgress * 100))
                        : qsTr("whisper.cpp program: %1").arg(Lyrics.sttEngineReady ? qsTr("installed")
                              : Lyrics.sttCanDownloadEngine ? qsTr("missing (official release from GitHub)")
                              : qsTr("missing — install the whisper.cpp package (whisper-cli or whisper-cpp command)"))
                          + " · " + qsTr("model %1: %2").arg(Lyrics.sttModel).arg(Lyrics.sttModelReady ? qsTr("downloaded") : qsTr("missing (%1, Hugging Face)").arg(Lyrics.modelSize(Lyrics.sttModel)))
                          + ". " + qsTr("Files are verified with SHA-256.")
                    Row {
                        spacing: 8
                        PillButton {
                            visible: Lyrics.sttDownloading
                            kind: "ghost"
                            icon: "close"
                            text: qsTr("Cancel")
                            onClicked: Lyrics.cancelSttDownload()
                        }
                        PillButton {
                            visible: !Lyrics.sttDownloading && !Lyrics.sttEngineReady && Lyrics.sttCanDownloadEngine
                            kind: "primary"
                            icon: "add"
                            text: qsTr("Download whisper.cpp")
                            onClicked: Lyrics.downloadSttEngine()
                        }
                        PillButton {
                            visible: !Lyrics.sttDownloading && !Lyrics.sttEngineReady && !Lyrics.sttCanDownloadEngine
                            kind: "ghost"
                            icon: "refresh"
                            text: qsTr("Check again")
                            onClicked: Lyrics.redetectStt()
                        }
                        PillButton {
                            visible: !Lyrics.sttDownloading && Lyrics.sttEngineReady && !Lyrics.sttModelReady
                            kind: "primary"
                            icon: "add"
                            text: qsTr("Download the model")
                            onClicked: Lyrics.downloadSttModel()
                        }
                        PillButton {
                            visible: !Lyrics.sttDownloading && Lyrics.sttModelReady
                            kind: "ghost"
                            text: qsTr("Remove the model")
                            onClicked: Lyrics.removeSttModel(Lyrics.sttModel)
                        }
                    }
                }
            }
        }

        Reveal {
            delay: 165
            ColumnLayout {
                width: parent.width
                spacing: 8
                Heading { text: "Discord" }
                SettingRow {
                    icon: "chat"
                    title: qsTr("Show what you're listening to on Discord")
                    description: qsTr("A “Listening” status with the title, artist, cover and a progress bar. Works with the Discord app running on this computer. Status: %1.").arg(Discord.status)
                    Toggle { checked: Discord.enabled; onToggled: c => Discord.enabled = c }
                }
                SettingRow {
                    visible: Discord.enabled
                    icon: "key"
                    title: qsTr("Discord application ID")
                    description: Discord.builtInClientId && Discord.clientId !== ""
                        ? qsTr("Nokturn's built-in ID. You can enter your own to change the name shown in the status.")
                        : qsTr("Create an application at discord.com/developers/applications (its name will show as “Listening to <name>”), copy the “Application ID” and paste it here. Optionally add an image named “nokturn” under Rich Presence → Art Assets — it becomes the cover for files without a cover online.")
                    InputField {
                        width: 230
                        implicitHeight: 38
                        placeholderText: qsTr("e.g. 1234567890123456789")
                        text: Discord.clientId
                        inputMethodHints: Qt.ImhDigitsOnly
                        onEditingFinished: Discord.clientId = text
                    }
                }
                SettingRow {
                    visible: Discord.enabled
                    icon: "pause"
                    title: qsTr("Show while paused too")
                    description: qsTr("By default the status disappears when the music is paused (like Spotify).")
                    Toggle { checked: Discord.showWhenPaused; onToggled: c => Discord.showWhenPaused = c }
                }
            }
        }

        Reveal {
            delay: 180
            ColumnLayout {
                width: parent.width
                spacing: 8
                Heading { text: qsTr("Stream quality") }
                SettingRow {
                    icon: "playCircle"
                    title: qsTr("YouTube Music and radio")
                    description: [qsTr("Lowest data usage."), qsTr("Balanced quality."), qsTr("Highest available quality (up to 256 kbps)."), qsTr("Lossless when the source allows it.")][page.quality]
                    SegmentedTabs {
                        model: [qsTr("Data saver"), qsTr("Normal"), qsTr("High"), qsTr("Lossless")]
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
                Heading { text: qsTr("Keyboard shortcuts") }
                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 30
                    rowSpacing: 12

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
                    Layout.fillWidth: true
                    spacing: 2
                    Text { text: "Nokturn " + Updater.currentVersion; color: Theme.text; font.pixelSize: 14; font.weight: Font.DemiBold }
                    Text { text: "C++20 · Qt 6 · Qt Quick"; color: Theme.textFaint; font.pixelSize: 12 }
                }
                PillButton {
                    text: Updater.available ? qsTr("Update to %1").arg(Updater.latestVersion) : qsTr("Check for updates")
                    kind: "secondary"
                    icon: "refresh"
                    busy: Updater.state === "checking"
                    onClicked: Updater.check(true)
                }
            }
        }
    }
}
