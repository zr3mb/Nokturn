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
                    description: Player.crossfade > 0
                        ? "Koniec utworu nakłada się na początek następnego. Playlisty z włączonym „Miksem” używają własnych ustawień."
                        : "Wyłączone — utwory grają po kolei. Miks możesz też włączyć osobno dla wybranej playlisty."
                    Row {
                        spacing: 14
                        ProgressSlider {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 170
                            value: Player.crossfade / 12
                            labelFor: function(v) { return Math.round(v * 12) === 0 ? "wył." : Math.round(v * 12) + " s" }
                            onMoved: v => Player.crossfade = Math.round(v * 12)
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 34
                            text: Player.crossfade === 0 ? "wył." : Player.crossfade + " s"
                            color: Theme.textDim
                            font.pixelSize: 13
                            font.features: { "tnum": 1 }
                        }
                    }
                }
                SettingRow {
                    visible: Player.crossfade > 0
                    icon: "sparkle"
                    title: "Styl przejścia"
                    description: Player.mixStyleDescription(Player.crossfadeStyle)
                    SegmentedTabs {
                        model: Player.mixStyles.map(s => Player.mixStyleName(s))
                        currentIndex: Player.mixStyles.indexOf(Player.crossfadeStyle)
                        onActivated: i => Player.crossfadeStyle = Player.mixStyles[i]
                    }
                }
                SettingRow {
                    icon: "next"
                    title: "Inteligentne przejścia"
                    description: "Miks zaczyna się, gdy muzyka faktycznie się kończy: pomija ciszę na końcu i na początku utworów oraz wykrywa wyciszenia zapisane w nagraniu."
                    Toggle { checked: Player.smartTransitions; onToggled: c => Player.smartTransitions = c }
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
            delay: 150
            ColumnLayout {
                width: parent.width
                spacing: 8
                Heading { text: "Teksty utworów" }
                SettingRow {
                    icon: "lyrics"
                    title: "Teksty z YouTube Music"
                    description: "Tekst odtwarzanego utworu (z synchronizacją, gdy jest dostępna) w panelu „Teraz odtwarzane” i w trybie pełnoekranowym. Dla plików lokalnych utwór jest wyszukiwany po wykonawcy i tytule; awaryjnie używany jest plik .lrc obok utworu albo tekst zapisany w tagach."
                }
                SettingRow {
                    icon: "sparkle"
                    title: "Automatyczne dopasowanie czasu (alpha)"
                    description: Lyrics.hardwareOk
                        ? "Gdy tekst nie ma znaczników czasu, Nokturn rozpozna śpiew w nagraniu (whisper.cpp, lokalnie na Twoim komputerze) i dopasuje do niego linie tekstu. Mocno obciąża procesor przez kilkadziesiąt sekund do kilku minut na utwór; wynik jest zapamiętywany. Twój sprzęt: " + Lyrics.hardwareSummary + "."
                        : "Wymaga mocniejszego sprzętu (co najmniej 4 wątki procesora i 8 GB RAM). Twój sprzęt: " + Lyrics.hardwareSummary + "."
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
                    title: "Model rozpoznawania mowy"
                    description: "Większy model lepiej rozpoznaje śpiew, ale działa wolniej. "
                        + Lyrics.sttModel + ": " + Lyrics.modelSize(Lyrics.sttModel)
                        + (Lyrics.sttModel === Lyrics.recommendedModel ? " · zalecany dla Twojego sprzętu" : " · zalecany: " + Lyrics.recommendedModel)
                    SegmentedTabs {
                        model: Lyrics.sttModels.map(m => m.charAt(0).toUpperCase() + m.slice(1))
                        currentIndex: Lyrics.sttModels.indexOf(Lyrics.sttModel)
                        onActivated: i => Lyrics.sttModel = Lyrics.sttModels[i]
                    }
                }
                SettingRow {
                    visible: Lyrics.autoAlign
                    icon: Lyrics.sttReady ? "check" : "add"
                    title: Lyrics.sttReady ? "Rozpoznawanie mowy jest gotowe" : "Pobierz składniki"
                    description: Lyrics.sttDownloading
                        ? "Pobieranie: " + Lyrics.sttDownloadLabel + "… " + Math.round(Lyrics.sttProgress * 100) + "%"
                        : "Program whisper.cpp: " + (Lyrics.sttEngineReady ? "zainstalowany"
                              : Lyrics.sttCanDownloadEngine ? "brak (oficjalne wydanie z GitHuba)"
                              : "brak — zainstaluj pakiet whisper.cpp (polecenie whisper-cli lub whisper-cpp)")
                          + " · model " + Lyrics.sttModel + ": " + (Lyrics.sttModelReady ? "pobrany" : "brak (" + Lyrics.modelSize(Lyrics.sttModel) + ", Hugging Face)")
                          + ". Pliki są sprawdzane sumą SHA-256."
                    Row {
                        spacing: 8
                        PillButton {
                            visible: Lyrics.sttDownloading
                            kind: "ghost"
                            icon: "close"
                            text: "Przerwij"
                            onClicked: Lyrics.cancelSttDownload()
                        }
                        PillButton {
                            visible: !Lyrics.sttDownloading && !Lyrics.sttEngineReady && Lyrics.sttCanDownloadEngine
                            kind: "primary"
                            icon: "add"
                            text: "Pobierz whisper.cpp"
                            onClicked: Lyrics.downloadSttEngine()
                        }
                        PillButton {
                            visible: !Lyrics.sttDownloading && !Lyrics.sttEngineReady && !Lyrics.sttCanDownloadEngine
                            kind: "ghost"
                            icon: "refresh"
                            text: "Sprawdź ponownie"
                            onClicked: Lyrics.redetectStt()
                        }
                        PillButton {
                            visible: !Lyrics.sttDownloading && Lyrics.sttEngineReady && !Lyrics.sttModelReady
                            kind: "primary"
                            icon: "add"
                            text: "Pobierz model"
                            onClicked: Lyrics.downloadSttModel()
                        }
                        PillButton {
                            visible: !Lyrics.sttDownloading && Lyrics.sttModelReady
                            kind: "ghost"
                            text: "Usuń model"
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
                    title: "Pokazuj na Discordzie, czego słuchasz"
                    description: "Status „Słucha” z tytułem, wykonawcą, okładką i paskiem postępu. Działa z uruchomioną aplikacją Discord na tym komputerze. Stan: " + Discord.status + "."
                    Toggle { checked: Discord.enabled; onToggled: c => Discord.enabled = c }
                }
                SettingRow {
                    visible: Discord.enabled
                    icon: "key"
                    title: "Identyfikator aplikacji Discord"
                    description: Discord.builtInClientId && Discord.clientId !== ""
                        ? "Wbudowany identyfikator Nokturna. Możesz wpisać własny, żeby zmienić nazwę wyświetlaną w statusie."
                        : "Utwórz aplikację na discord.com/developers/applications (jej nazwa pojawi się jako „Słucha <nazwa>”), skopiuj „Application ID” i wklej go tutaj. Opcjonalnie dodaj w Rich Presence → Art Assets obrazek o nazwie „nokturn” — będzie okładką plików bez okładki w sieci."
                    InputField {
                        width: 230
                        implicitHeight: 38
                        placeholderText: "np. 1234567890123456789"
                        text: Discord.clientId
                        inputMethodHints: Qt.ImhDigitsOnly
                        onEditingFinished: Discord.clientId = text
                    }
                }
                SettingRow {
                    visible: Discord.enabled
                    icon: "pause"
                    title: "Pokazuj także po wstrzymaniu"
                    description: "Domyślnie status znika, gdy muzyka jest wstrzymana (jak w Spotify)."
                    Toggle { checked: Discord.showWhenPaused; onToggled: c => Discord.showWhenPaused = c }
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
