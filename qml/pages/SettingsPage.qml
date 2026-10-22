import QtQuick
import QtQuick.Dialogs
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
                ThemePicker {
                    Layout.fillWidth: true
                    Layout.bottomMargin: 6
                }
                SettingRow {
                    icon: "image"
                    title: qsTr("Background image")
                    description: Theme.wallpaper !== ""
                        ? decodeURIComponent(Theme.wallpaper.replace(/^.*[\/]/, ""))
                        : qsTr("Your own picture behind the interface — in any theme.")
                    Row {
                        spacing: 8
                        PillButton {
                            visible: Theme.wallpaper !== ""
                            kind: "ghost"
                            text: qsTr("Remove")
                            fontSize: 13
                            implicitHeight: 36
                            onClicked: Theme.wallpaper = ""
                        }
                        PillButton {
                            kind: "secondary"
                            icon: "image"
                            text: Theme.wallpaper !== "" ? qsTr("Change") : qsTr("Choose a picture")
                            fontSize: 13
                            implicitHeight: 36
                            onClicked: wallpaperDialog.open()
                        }
                    }
                }
                SettingRow {
                    visible: Theme.wallpaper !== ""
                    icon: "tune"
                    title: qsTr("Background dimming")
                    description: qsTr("How much the picture is darkened so the text stays readable.")
                    ProgressSlider {
                        width: 170
                        value: Theme.wallpaperDim
                        labelFor: function(v) { return Math.round(v * 100) + "%" }
                        onMoved: v => Theme.wallpaperDim = Math.round(v * 100) / 100
                    }
                }
                SettingRow {
                    icon: "refresh"
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
                    icon: "image"
                    title: qsTr("Dynamic accent color")
                    description: Theme.preset.fixedAccent
                        ? qsTr("The %1 theme keeps its own colors.").arg(Theme.preset.name)
                        : qsTr("Interface colors smoothly adapt to the cover of the playing track.")
                    Toggle {
                        enabled: !Theme.preset.fixedAccent
                        opacity: enabled ? 1 : 0.4
                        checked: Theme.dynamicAccent
                        onToggled: c => Theme.dynamicAccent = c
                    }
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
                    icon: "mix"
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

        // The in-game overlay (GameOverlay, qml/overlay) — Windows only.
        Reveal {
            delay: 135
            visible: GameOverlay.supported
            ColumnLayout {
                width: parent.width
                spacing: 8
                Heading { text: qsTr("In-game overlay") }
                SettingRow {
                    icon: "gamepad"
                    title: qsTr("Music menu over games")
                    description: qsTr("A menu over the dimmed game to pause, skip, change the volume or switch the playlist without leaving it. Games in exclusive full screen are minimized while it's open — borderless or windowed mode works best.")
                    Row {
                        spacing: 12
                        PillButton {
                            anchors.verticalCenter: parent.verticalCenter
                            visible: GameOverlay.enabled
                            kind: "ghost"
                            text: qsTr("Try it")
                            onClicked: GameOverlay.show()
                        }
                        Toggle {
                            anchors.verticalCenter: parent.verticalCenter
                            checked: GameOverlay.enabled
                            onToggled: c => GameOverlay.enabled = c
                        }
                    }
                }
                SettingRow {
                    visible: GameOverlay.enabled && GameOverlay.hotkeyMode === "native"
                    icon: "key"
                    title: qsTr("Shortcut")
                    description: hotkeyField.recording ? qsTr("Press the new shortcut (with Ctrl or Alt, or an F key). Esc cancels.")
                               : hotkeyField.rejected ? qsTr("That can't be a shortcut — add Ctrl or Alt, or use an F key.")
                               : GameOverlay.hotkeyTaken ? qsTr("Another app already uses %1 — pick a different shortcut.").arg(GameOverlay.hotkey)
                               : qsTr("Opens and closes the menu from any game. Click to change it.")
                    Row {
                        spacing: 12
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            visible: GameOverlay.hotkey !== "Ctrl+Shift+M" && !hotkeyField.recording
                            text: qsTr("Reset")
                            color: resetMouse.containsMouse ? Theme.text : Theme.accent
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            MouseArea {
                                id: resetMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: GameOverlay.resetHotkey()
                            }
                        }
                        // The shortcut as a key cap; click, then press the new one.
                        Rectangle {
                            id: hotkeyField
                            property bool recording: false
                            property bool rejected: false
                            anchors.verticalCenter: parent.verticalCenter
                            width: Math.max(130, hotkeyText.implicitWidth + 28)
                            height: 34
                            radius: 9
                            color: recording ? Theme.alpha(Theme.accent, 0.22) : Theme.alpha(Theme.text, keyMouse.containsMouse ? 0.1 : 0.06)
                            border.width: 1
                            border.color: recording ? Theme.accent : GameOverlay.hotkeyTaken ? Theme.danger : Theme.alpha(Theme.text, 0.14)
                            onActiveFocusChanged: if (!activeFocus) recording = false
                            Text {
                                id: hotkeyText
                                anchors.centerIn: parent
                                text: hotkeyField.recording ? qsTr("Press keys…") : GameOverlay.hotkey
                                color: Theme.text
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                            }
                            MouseArea {
                                id: keyMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    hotkeyField.rejected = false
                                    hotkeyField.recording = true
                                    hotkeyField.forceActiveFocus()
                                }
                            }
                            Keys.onPressed: event => {
                                if (!recording)
                                    return
                                event.accepted = true
                                if (event.key === Qt.Key_Escape) {
                                    recording = false
                                    return
                                }
                                // Modifiers alone: wait for the key.
                                if ([Qt.Key_Control, Qt.Key_Shift, Qt.Key_Alt, Qt.Key_Meta, Qt.Key_AltGr].indexOf(event.key) >= 0)
                                    return
                                rejected = !GameOverlay.setHotkey(event.key, event.modifiers)
                                recording = false
                            }
                        }
                    }
                }
                // Linux: the desktop owns the shortcut (portal) …
                SettingRow {
                    visible: GameOverlay.enabled && GameOverlay.hotkeyMode === "portal"
                    icon: "key"
                    title: qsTr("Shortcut")
                    description: GameOverlay.hotkey !== ""
                        ? qsTr("Set in your desktop's shortcut settings — it opens and closes the menu from any game.")
                        : qsTr("Your desktop asks once whether Nokturn may have a shortcut. On Hyprland, add to hyprland.conf: %1")
                              .arg("bind = CTRL SHIFT, M, global, nokturn:overlay")
                    Row {
                        spacing: 12
                        PillButton {
                            anchors.verticalCenter: parent.verticalCenter
                            visible: GameOverlay.canConfigureHotkey
                            kind: "ghost"
                            text: qsTr("Change")
                            onClicked: GameOverlay.configureHotkey()
                        }
                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            visible: GameOverlay.hotkey !== ""
                            width: portalKey.implicitWidth + 28
                            height: 34
                            radius: 9
                            color: Theme.alpha(Theme.text, 0.06)
                            border.width: 1
                            border.color: Theme.alpha(Theme.text, 0.14)
                            Text {
                                id: portalKey
                                anchors.centerIn: parent
                                text: GameOverlay.hotkey
                                color: Theme.text
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                            }
                        }
                    }
                }
                // … or there's no portal: a command for the desktop's own shortcut settings.
                SettingRow {
                    visible: GameOverlay.enabled && GameOverlay.hotkeyMode === "command"
                    icon: "key"
                    title: qsTr("Shortcut")
                    description: qsTr("Your desktop doesn't let apps add shortcuts. Add one in its keyboard settings that runs: %1")
                                 .arg(GameOverlay.toggleCommand)
                    PillButton {
                        kind: "ghost"
                        text: qsTr("Copy command")
                        onClicked: {
                            GameOverlay.copyToClipboard(GameOverlay.toggleCommand)
                            Nav.toast(qsTr("Command copied"), "check")
                        }
                    }
                }
                SettingRow {
                    visible: GameOverlay.enabled
                    icon: "gamepad"
                    title: qsTr("Xbox controller")
                    description: (GameOverlay.padConnected ? qsTr("Controller connected.") : qsTr("No controller connected right now.")) + " "
                                 + qsTr("Hold View + Menu to open the menu; d-pad or stick to move, A selects, B goes back, Y pauses, LB / RB skip tracks.")
                    Toggle { checked: GameOverlay.padEnabled; onToggled: c => GameOverlay.padEnabled = c }
                }
                SettingRow {
                    icon: "lyrics"
                    title: qsTr("In-game lyrics overlay")
                    description: qsTr("Displays song lyrics directly over games while playing with click-through transparency. Won't intercept mouse or keyboard inputs.")
                    Row {
                        spacing: 12
                        PillButton {
                            anchors.verticalCenter: parent.verticalCenter
                            visible: GameOverlay.lyricsEnabled
                            kind: "ghost"
                            text: qsTr("Try it")
                            onClicked: GameOverlay.showLyrics()
                        }
                        Toggle {
                            anchors.verticalCenter: parent.verticalCenter
                            checked: GameOverlay.lyricsEnabled
                            onToggled: c => GameOverlay.lyricsEnabled = c
                        }
                    }
                }
                SettingRow {
                    visible: GameOverlay.lyricsEnabled
                    icon: "tune"
                    title: qsTr("Lyrics display style")
                    description: GameOverlay.lyricsMode === "sections"
                        ? qsTr("Sections: shows verse stanzas in clear blocks, highlighting the active singing line.")
                        : qsTr("Karaoke: active line prominent in the center with letter-by-letter timing and next-line preview.")
                    SegmentedTabs {
                        model: [qsTr("Karaoke"), qsTr("Sections")]
                        currentIndex: GameOverlay.lyricsMode === "sections" ? 1 : 0
                        onActivated: i => GameOverlay.lyricsMode = (i === 1 ? "sections" : "karaoke")
                    }
                }
                SettingRow {
                    visible: GameOverlay.lyricsEnabled
                    icon: "eye"
                    title: qsTr("Background dimming")
                    description: qsTr("Slight screen darkening over the game to make lyrics easy to read: %1%.").arg(Math.round(GameOverlay.lyricsDimming * 100))
                    Row {
                        spacing: 12
                        anchors.verticalCenter: parent.verticalCenter
                        ProgressSlider {
                            anchors.verticalCenter: parent.verticalCenter
                            implicitWidth: 160
                            value: GameOverlay.lyricsDimming / 0.80
                            onMoved: v => GameOverlay.lyricsDimming = Math.round(v * 0.80 * 100) / 100
                            labelFor: v => Math.round(v * 80) + "%"
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: Math.round(GameOverlay.lyricsDimming * 100) + "%"
                            color: Theme.textDim
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            width: 36
                        }
                    }
                }
                SettingRow {
                    visible: GameOverlay.lyricsEnabled
                    icon: "menuOpen"
                    title: qsTr("Screen position")
                    description: qsTr("Where the lyrics HUD appears on your monitor.")
                    SegmentedTabs {
                        model: [qsTr("Bottom"), qsTr("Center"), qsTr("Top")]
                        currentIndex: ["bottom", "center", "top"].indexOf(GameOverlay.lyricsPosition) >= 0
                            ? ["bottom", "center", "top"].indexOf(GameOverlay.lyricsPosition) : 0
                        onActivated: i => GameOverlay.lyricsPosition = ["bottom", "center", "top"][i]
                    }
                }
                SettingRow {
                    visible: GameOverlay.lyricsEnabled && GameOverlay.hotkeyMode === "native"
                    icon: "key"
                    title: qsTr("Lyrics shortcut")
                    description: lyricsHotkeyField.recording ? qsTr("Press the new shortcut (with Ctrl or Alt, or an F key). Esc cancels.")
                               : lyricsHotkeyField.rejected ? qsTr("That can't be a shortcut — add Ctrl or Alt, or use an F key.")
                               : GameOverlay.lyricsHotkeyTaken ? qsTr("Another app already uses %1 — pick a different shortcut.").arg(GameOverlay.lyricsHotkey)
                               : qsTr("Toggles the in-game lyrics overlay on and off while playing.")
                    Row {
                        spacing: 12
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            visible: GameOverlay.lyricsHotkey !== "Ctrl+Shift+L" && !lyricsHotkeyField.recording
                            text: qsTr("Reset")
                            color: lyricsResetMouse.containsMouse ? Theme.text : Theme.accent
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            MouseArea {
                                id: lyricsResetMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: GameOverlay.resetLyricsHotkey()
                            }
                        }
                        Rectangle {
                            id: lyricsHotkeyField
                            property bool recording: false
                            property bool rejected: false
                            anchors.verticalCenter: parent.verticalCenter
                            width: Math.max(130, lyricsHotkeyText.implicitWidth + 28)
                            height: 34
                            radius: 9
                            color: recording ? Theme.alpha(Theme.accent, 0.22) : Theme.alpha(Theme.text, lyricsKeyMouse.containsMouse ? 0.1 : 0.06)
                            border.width: 1
                            border.color: recording ? Theme.accent : GameOverlay.lyricsHotkeyTaken ? Theme.danger : Theme.alpha(Theme.text, 0.14)
                            onActiveFocusChanged: if (!activeFocus) recording = false
                            Text {
                                id: lyricsHotkeyText
                                anchors.centerIn: parent
                                text: lyricsHotkeyField.recording ? qsTr("Press keys…") : GameOverlay.lyricsHotkey
                                color: Theme.text
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                            }
                            MouseArea {
                                id: lyricsKeyMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    lyricsHotkeyField.rejected = false
                                    lyricsHotkeyField.recording = true
                                    lyricsHotkeyField.forceActiveFocus()
                                }
                            }
                            Keys.onPressed: event => {
                                if (!recording)
                                    return
                                event.accepted = true
                                if (event.key === Qt.Key_Escape) {
                                    recording = false
                                    return
                                }
                                if ([Qt.Key_Control, Qt.Key_Shift, Qt.Key_Alt, Qt.Key_Meta, Qt.Key_AltGr].indexOf(event.key) >= 0)
                                    return
                                rejected = !GameOverlay.setLyricsHotkey(event.key, event.modifiers)
                                recording = false
                            }
                        }
                    }
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
                    icon: "lyrics"
                    title: qsTr("Letter-by-letter highlight")
                    description: qsTr("The line being sung lights up letter by letter, karaoke style. Off: the whole line lights up at once, and words aren't timed.")
                    Toggle { checked: Lyrics.karaoke; onToggled: c => Lyrics.karaoke = c }
                }
                SettingRow {
                    icon: "clock"
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
                // Shown with the hardware for it: recognizing lyrics (a button in the lyrics) needs them too.
                SettingRow {
                    visible: Lyrics.hardwareOk
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
                    visible: Lyrics.hardwareOk
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
                // An NVIDIA card: recognition runs there, several times faster
                SettingRow {
                    visible: Lyrics.sttGpuName !== ""
                    icon: "equalizer"
                    title: qsTr("Graphics card acceleration")
                    description: !Lyrics.sttGpuSupported
                        ? qsTr("%1 could recognize the singing much faster, but it needs NVIDIA driver 551 or newer (yours: %2).").arg(Lyrics.sttGpuName).arg(Lyrics.sttGpuDriver)
                        : Lyrics.sttGpuEngineReady
                          ? qsTr("Speech recognition runs on %1: several times faster than on the CPU, and the largest model is quick too.").arg(Lyrics.sttGpuName)
                          : qsTr("%1 can recognize the singing several times faster than the CPU. It needs the CUDA version of whisper.cpp (official release from GitHub, %2, verified with SHA-256).").arg(Lyrics.sttGpuName).arg(Lyrics.sttGpuEngineSize)
                    Row {
                        spacing: 8
                        PillButton {
                            visible: Lyrics.sttGpuSupported && !Lyrics.sttGpuEngineReady && !Lyrics.sttDownloading
                            kind: "primary"
                            icon: "add"
                            text: qsTr("Download")
                            onClicked: Lyrics.downloadSttGpuEngine()
                        }
                        PillButton {
                            visible: Lyrics.sttGpuEngineReady && !Lyrics.sttDownloading
                            kind: "ghost"
                            text: qsTr("Remove")
                            onClicked: Lyrics.removeSttGpuEngine()
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

    FileDialog {
        id: wallpaperDialog
        title: qsTr("Choose a background picture")
        nameFilters: [qsTr("Images (*.jpg *.jpeg *.png *.webp *.bmp *.gif)")]
        onAccepted: Theme.wallpaper = selectedFile.toString()
    }
}
