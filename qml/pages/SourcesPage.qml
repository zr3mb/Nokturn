import QtQuick
import QtQuick.Layouts
import Nokturn

// Zarządzanie źródłami muzyki. Pliki lokalne i YouTube Music działają naprawdę;
// radio i własne strumienie są na razie tylko interfejsem.
Item {
    id: page
    property var args: ({})

    function src(id) { return Library.sources.find(s => s.id === id) || {} }



    component ListLine: Item {
        id: line
        property string icon: "folder"
        property color iconColor: Theme.textDim
        property string title: ""
        property string detail: ""
        property bool live: false
        signal activated
        Layout.fillWidth: true
        implicitHeight: 46

        HoverHandler { id: lineHover }
        Rectangle {
            anchors.fill: parent
            radius: 12
            color: Theme.alpha(Theme.text, lineHover.hovered ? 0.06 : 0.03)
            Behavior on color { ColorAnimation { duration: Theme.fast } }
        }
        Icon {
            id: lineIcon
            x: 14
            anchors.verticalCenter: parent.verticalCenter
            name: line.icon
            size: 18
            color: line.iconColor
        }
        Rectangle {
            visible: line.live
            anchors { left: lineIcon.right; leftMargin: -4; top: lineIcon.top; topMargin: -2 }
            width: 7; height: 7; radius: 3.5
            color: Theme.danger
            SequentialAnimation on opacity {
                running: line.live
                loops: Animation.Infinite
                NumberAnimation { to: 0.25; duration: 700; easing.type: Easing.InOutSine }
                NumberAnimation { to: 1; duration: 700; easing.type: Easing.InOutSine }
            }
        }
        Text {
            anchors { left: lineIcon.right; leftMargin: 12; right: lineDetail.left; rightMargin: 10; verticalCenter: parent.verticalCenter }
            text: line.title
            color: Theme.text
            font.pixelSize: 14
            font.weight: Font.Medium
            elide: Text.ElideMiddle
        }
        Text {
            id: lineDetail
            anchors { right: lineAction.left; rightMargin: 6; verticalCenter: parent.verticalCenter }
            text: line.detail
            color: Theme.textFaint
            font.pixelSize: 12
        }
        IconButton {
            id: lineAction
            anchors { right: parent.right; rightMargin: 4; verticalCenter: parent.verticalCenter }
            width: 32; height: 32
            icon: line.live ? "play" : "close"
            iconSize: 16
            showDot: false
            opacity: lineHover.hovered ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: Theme.fast } }
            onClicked: line.activated()
        }
    }

    component Hint: Text {
        Layout.fillWidth: true
        color: Theme.textDim
        font.pixelSize: 13
        wrapMode: Text.WordWrap
        lineHeight: 1.2
    }

    PageFlick {
        anchors.fill: parent

        Reveal {
            ColumnLayout {
                width: parent.width
                spacing: 4
                Text {
                    text: "Źródła muzyki"
                    color: Theme.text
                    font.family: Theme.fontDisplay
                    font.pixelSize: 38
                    font.weight: Font.Bold
                }
                Text {
                    text: "Połącz miejsca, z których Nokturn ma odtwarzać muzykę. Wszystko trafi do jednej biblioteki."
                    color: Theme.textDim
                    font.pixelSize: 15
                }
            }
        }

        Reveal {
            delay: 80
            // Dwie kolumny kart na szerokim oknie, jedna na wąskim
            GridLayout {
                width: parent.width
                columns: width > 900 ? 2 : 1
                columnSpacing: 18
                rowSpacing: 18

                // ---- lewa kolumna --------------------------------------------
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 100
                    Layout.alignment: Qt.AlignTop
                    spacing: 18

                    SourceCard {
                        source: page.src("local")
                        status: Library.scanning ? "connecting" : Library.folders.length > 0 ? "connected" : "disconnected"
                        detail: Library.folders.length === 0 ? ""
                                : Theme.plural(Library.folders.length, "folder", "foldery", "folderów") + " · "
                                  + Theme.plural(Library.localCount, "utwór", "utwory", "utworów")

                        Hint {
                            visible: Library.folders.length === 0
                            text: "Nie wybrano jeszcze żadnego folderu. Dodaj folder, a Nokturn przeskanuje go razem z podfolderami."
                        }

                        Repeater {
                            model: Library.folders
                            ListLine {
                                required property var modelData
                                icon: "folder"
                                iconColor: "#9fbfa8"
                                title: modelData.path
                                detail: Theme.plural(modelData.count, "utwór", "utwory", "utworów")
                                onActivated: Library.removeFolder(modelData.path)
                            }
                        }

                        Item {
                            Layout.fillWidth: true
                            implicitHeight: Library.scanning ? 38 : 0
                            opacity: Library.scanning ? 1 : 0
                            clip: true
                            Behavior on implicitHeight { NumberAnimation { duration: 380; easing.type: Easing.OutQuint } }
                            Behavior on opacity { NumberAnimation { duration: 300 } }

                            Text {
                                text: Library.scanStatus
                                color: Theme.textDim
                                font.pixelSize: 12
                                font.features: { "tnum": 1 }
                            }
                            Rectangle {
                                id: scanTrack
                                anchors { left: parent.left; right: parent.right; bottom: parent.bottom; bottomMargin: 6 }
                                height: 6
                                radius: 3
                                color: Theme.alpha(Theme.text, 0.08)
                                clip: true

                                Rectangle {
                                    width: parent.width * Library.scanProgress
                                    height: parent.height
                                    radius: 3
                                    gradient: Gradient {
                                        orientation: Gradient.Horizontal
                                        GradientStop { position: 0; color: Qt.darker("#9fbfa8", 1.3) }
                                        GradientStop { position: 1; color: "#9fbfa8" }
                                    }
                                }
                                Rectangle {
                                    id: shimmer
                                    width: 90
                                    height: parent.height
                                    gradient: Gradient {
                                        orientation: Gradient.Horizontal
                                        GradientStop { position: 0; color: "#00ffffff" }
                                        GradientStop { position: 0.5; color: "#55ffffff" }
                                        GradientStop { position: 1; color: "#00ffffff" }
                                    }
                                    NumberAnimation on x {
                                        running: Library.scanning
                                        from: -90; to: scanTrack.width
                                        duration: 1100
                                        loops: Animation.Infinite
                                    }
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            PillButton {
                                kind: "secondary"
                                icon: "add"
                                text: "Dodaj folder"
                                onClicked: Nav.addFolder()
                            }
                            PillButton {
                                kind: "ghost"
                                icon: "refresh"
                                text: Library.scanning ? "Skanuję…" : "Skanuj ponownie"
                                busy: Library.scanning
                                enabled: Library.folders.length > 0
                                onClicked: Library.rescan()
                            }
                        }
                    }

                    SourceCard {
                        source: page.src("youtube")
                        status: YouTube.loggingIn ? "connecting" : YouTube.connected ? "connected" : "disconnected"
                        detail: !YouTube.connected ? ""
                                : YouTube.syncing ? "Synchronizacja z kontem…"
                                : Theme.plural(YouTube.likedCount, "polubiony utwór", "polubione utwory", "polubionych utworów")
                                  + " · " + Theme.plural(YouTube.playlistCount, "playlista", "playlisty", "playlist")
                                  + (YouTube.lastSync !== "" ? " · " + YouTube.lastSync : "")

                        // Niezalogowany
                        ColumnLayout {
                            visible: !YouTube.connected && !YouTube.loggingIn
                            Layout.fillWidth: true
                            spacing: 12

                            Hint {
                                text: YouTube.browserName !== ""
                                      ? "Otworzymy osobne okno " + YouTube.browserName + " z czystym profilem. Zaloguj się w nim "
                                        + "normalnie na swoje konto Google. Gdy pojawi się YouTube Music, okno zamknie się samo, a Nokturn pobierze polubione utwory i playlisty."
                                      : "Nie znaleziono Chrome, Edge ani Brave. Możesz wkleić ciasteczka ręcznie."
                            }
                            Flow {
                                Layout.fillWidth: true
                                spacing: 10
                                PillButton {
                                    icon: "person"
                                    text: "Zaloguj się przez przeglądarkę"
                                    enabled: YouTube.browserName !== ""
                                    onClicked: YouTube.login()
                                }
                                PillButton {
                                    kind: "ghost"
                                    icon: "key"
                                    text: manualLogin.visible ? "Ukryj" : "Wklej ciasteczka"
                                    onClicked: manualLogin.visible = !manualLogin.visible
                                }
                            }
                            ColumnLayout {
                                id: manualLogin
                                visible: false
                                Layout.fillWidth: true
                                spacing: 10
                                Hint {
                                    text: "Skopiuj nagłówek Cookie z zapytania do music.youtube.com (narzędzia deweloperskie → Sieć) i wklej go tutaj."
                                }
                                InputField {
                                    id: cookieField
                                    Layout.fillWidth: true
                                    icon: "key"
                                    echoMode: TextInput.Password
                                    placeholderText: "SAPISID=…; LOGIN_INFO=…; …"
                                }
                                PillButton {
                                    kind: "secondary"
                                    icon: "link"
                                    text: "Połącz"
                                    enabled: cookieField.text.length > 0
                                    onClicked: {
                                        YouTube.loginWithCookies(cookieField.text)
                                        cookieField.text = ""
                                    }
                                }
                            }
                        }

                        // Trwa logowanie w przeglądarce
                        RowLayout {
                            visible: YouTube.loggingIn
                            Layout.fillWidth: true
                            spacing: 14
                            Spinner { size: 22; color: Theme.warning }
                            Hint {
                                text: YouTube.loginStatus
                            }
                            PillButton {
                                kind: "ghost"
                                text: "Anuluj"
                                onClicked: YouTube.cancelLogin()
                            }
                        }

                        // Zalogowany
                        RowLayout {
                            visible: YouTube.connected
                            Layout.fillWidth: true
                            spacing: 12

                            ArtCover {
                                Layout.preferredWidth: 44
                                Layout.preferredHeight: 44
                                radius: 22
                                decorated: false
                                colorA: "#cf9d9d"
                                colorB: "#5c3a42"
                                source: YouTube.accountPhoto
                                Icon {
                                    anchors.centerIn: parent
                                    visible: !parent.hasPicture
                                    name: "person"
                                    size: 22
                                    color: "#f4efe8"
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Text {
                                    Layout.fillWidth: true
                                    text: YouTube.accountName || "Konto YouTube Music"
                                    color: Theme.text
                                    font.pixelSize: 14
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }
                                Text {
                                    Layout.fillWidth: true
                                    visible: text !== ""
                                    text: YouTube.accountHandle
                                    color: Theme.textDim
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                }
                            }
                            Spinner {
                                visible: YouTube.syncing
                                size: 18
                                color: Theme.textDim
                            }
                            IconButton {
                                visible: !YouTube.syncing
                                icon: "refresh"
                                iconSize: 18
                                showDot: false
                                onClicked: YouTube.refresh()
                            }
                            PillButton {
                                kind: "ghost"
                                text: "Wyloguj"
                                onClicked: YouTube.logout()
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.topMargin: 4
                            height: 1
                            color: Qt.rgba(1, 1, 1, 0.06)
                        }

                        // Silnik odtwarzania
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            Icon {
                                name: "playCircle"
                                size: 22
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
                                        : "Potrzebny do odtwarzania utworów z YouTube — pobierzemy oficjalne wydanie z GitHuba."
                                    color: Theme.textDim
                                    font.pixelSize: 12
                                    wrapMode: Text.WordWrap
                                }
                            }
                            PillButton {
                                kind: YouTube.engineReady ? "ghost" : "primary"
                                icon: YouTube.engineReady ? "refresh" : "add"
                                text: YouTube.engineReady ? "Aktualizuj" : "Pobierz"
                                busy: YouTube.engineDownloading
                                onClicked: YouTube.downloadEngine()
                            }
                        }
                    }
                }

                // ---- prawa kolumna -------------------------------------------
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 100
                    Layout.alignment: Qt.AlignTop
                    spacing: 18

                    SourceCard {
                        source: page.src("radio")

                        ListLine {
                            icon: "live"
                            iconColor: "#cdb68e"
                            live: true
                            title: "Ambient Waves FM — Nocny Jazz"
                            detail: "128 kbps"
                            onActivated: Player.playIndex(Library.search("", "radio", 1)[0])
                        }
                        ListLine {
                            icon: "live"
                            iconColor: "#cdb68e"
                            live: true
                            title: "Deep Focus Radio"
                            detail: "192 kbps"
                            onActivated: Nav.toast("Strumienie radiowe pojawią się wkrótce", "radio")
                        }
                        ListLine {
                            icon: "live"
                            iconColor: "#cdb68e"
                            live: true
                            title: "Lo-Fi Nocą"
                            detail: "128 kbps"
                            onActivated: Nav.toast("Strumienie radiowe pojawią się wkrótce", "radio")
                        }
                        PillButton {
                            kind: "ghost"
                            icon: "add"
                            text: "Dodaj stację"
                            onClicked: Nav.toast("Dodawanie stacji pojawi się wkrótce", "radio")
                        }
                    }

                    SourceCard {
                        source: page.src("stream")

                        Hint { text: "Odtwarzaj dowolny strumień audio — wklej adres pliku lub playlisty M3U / PLS." }
                        InputField {
                            id: urlField
                            Layout.fillWidth: true
                            icon: "link"
                            placeholderText: "https://…/stream.mp3"
                        }
                        PillButton {
                            icon: "play"
                            text: "Odtwórz"
                            enabled: urlField.text.length > 0
                            onClicked: Nav.toast("Odtwarzanie z adresu URL pojawi się wkrótce", "link")
                        }
                    }
                }
            }
        }
    }
}
