import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import Nokturn

ApplicationWindow {
    id: win
    readonly property bool mobile: Qt.platform.os === "android" || Qt.application.arguments.indexOf("--mobile") >= 0
    width: mobile ? 412 : 1420
    height: mobile ? 892 : 900
    minimumWidth: mobile ? 320 : 1120
    minimumHeight: mobile ? 480 : 720
    visible: true
    title: "Nokturn"
    color: Theme.bg
    flags: mobile ? Qt.Window : Qt.Window | Qt.FramelessWindowHint
    Component.onCompleted: {
        Nav.mobile = mobile
        // The first-run wizard is about the desktop window (layout, panels, integrations).
        if (mobile && Nav.onboarding)
            Nav.finishOnboarding()
    }

    // Android's back button: the full-screen player first, then the page history.
    onClosing: close => {
        if (!mobile)
            return
        if (moreSheet.opened) {
            moreSheet.close()
            close.accepted = false
        } else if (Nav.immersive) {
            Nav.immersive = false
            close.accepted = false
        } else if (Nav.canGoBack) {
            Nav.back()
            close.accepted = false
        }
    }

    readonly property bool maximized: visibility === Window.Maximized || visibility === Window.FullScreen
    onVisibilityChanged: visibility => { if (visibility === Window.Minimized) Memory.trimSoon(500) }

    // The whole window content in one item (also enables screenshots in --tour mode)
    Item {
        id: shell
        anchors.fill: parent

        Rectangle {
            anchors.fill: parent
            color: Theme.bg
        }

        // The mouse's side buttons go back / forward like in a browser.
        TapHandler {
            acceptedButtons: Qt.BackButton | Qt.ForwardButton
            gesturePolicy: TapHandler.WithinBounds
            onTapped: (eventPoint, button) => button === Qt.BackButton ? Nav.back() : Nav.forward()
        }

        ThemeBackdrop {
            anchors.fill: parent
        }

        AmbientBackground {
            anchors.fill: parent
            visible: Theme.ambient
        }

        TitleBar {
            id: titleBar
            anchors { top: parent.top; left: parent.left; right: parent.right }
            height: win.mobile ? 0 : 56
            visible: !win.mobile
        }

        Sidebar {
            id: sidebar
            anchors {
                top: titleBar.bottom; left: parent.left; bottom: playerBar.top
                leftMargin: win.mobile ? 0 : 10; bottomMargin: 2
            }
            width: win.mobile ? 0 : Nav.sidebarCompact ? 76 : 252
            visible: !win.mobile
            Behavior on width { enabled: !win.mobile; NumberAnimation { duration: 460; easing.type: Easing.OutQuint } }
        }

        Panel {
            id: content
            anchors {
                top: titleBar.bottom; left: sidebar.right; right: queuePanel.left
                bottom: win.mobile ? (mobilePlayer.visible ? mobilePlayer.top : mobileNav.top) : playerBar.top
                leftMargin: win.mobile ? 0 : 10; rightMargin: win.mobile ? 0 : 10; bottomMargin: win.mobile ? 6 : 2
            }
            radius: win.mobile ? 0 : 20
            roundedClip: !win.mobile
            tint: win.mobile ? "transparent" : Theme.alpha(Theme.surface, 0.66)

            StackView {
                id: stack
                anchors.fill: parent
                initialItem: homePage

                replaceEnter: Transition {
                    ParallelAnimation {
                        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 420; easing.type: Easing.OutCubic }
                        NumberAnimation { property: "y"; from: 34; to: 0; duration: 620; easing.type: Easing.OutQuint }
                        NumberAnimation { property: "scale"; from: 0.985; to: 1; duration: 620; easing.type: Easing.OutQuint }
                    }
                }
                replaceExit: Transition {
                    ParallelAnimation {
                        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 200; easing.type: Easing.InCubic }
                        NumberAnimation { property: "scale"; from: 1; to: 0.985; duration: 240; easing.type: Easing.InCubic }
                    }
                }
            }
        }

        NowPlayingPanel {
            id: queuePanel
            anchors {
                top: titleBar.bottom; right: parent.right; bottom: playerBar.top
                rightMargin: Nav.queueOpen ? 10 : 0; bottomMargin: 2
            }
            width: Nav.queueOpen && !win.mobile ? 344 : 0
            visible: !win.mobile
            Behavior on width { NumberAnimation { duration: 520; easing.type: Easing.OutQuint } }
            Behavior on anchors.rightMargin { NumberAnimation { duration: 520; easing.type: Easing.OutQuint } }
        }

        PlayerBar {
            id: playerBar
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: win.mobile ? 0 : 104
            visible: !win.mobile
        }

        MobilePlayerBar {
            id: mobilePlayer
            anchors { left: parent.left; right: parent.right; bottom: mobileNav.top; bottomMargin: 6 }
            height: 60
            visible: win.mobile && Player.hasTrack
        }

        MobileNav {
            id: mobileNav
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: win.mobile ? 64 : 0
            visible: win.mobile
            onMoreRequested: Nav.moreRequested(true)
        }

        ImmersiveView {
            anchors.fill: parent
        }

        // First-run wizard (above everything except notifications)
        Onboarding {
            anchors.fill: parent
        }

        // New version from GitHub Releases
        UpdateDialog {
            anchors.fill: parent
        }

        // Correcting a Spotify track's YouTube Music match. The dialogs are built when they open.
        Loader {
            id: matchDialog
            active: false
            sourceComponent: MatchDialog { onClosed: matchDialog.active = false }
            function show(track) {
                active = true
                item.show(track)
            }
        }

        // New playlist / editing one
        Loader {
            id: playlistDialog
            active: false
            sourceComponent: PlaylistDialog { onClosed: playlistDialog.active = false }
            function show(mode, index, rows) {
                active = true
                item.show(mode, index, rows)
            }
        }

        // Android: signing in inside the app (built only for the Android package)
        Loader {
            id: webLogin
            property string service: "youtube"
            anchors.fill: parent
            active: false
            source: "mobile/WebLogin.qml"
            onLoaded: item.service = service
            Connections {
                target: webLogin.item
                function onClosed() { webLogin.active = false }
            }
        }

        // A shared playlist's password (joining one that has it, or the owner setting it)
        Loader {
            id: passwordDialog
            active: false
            sourceComponent: PasswordDialog { onClosed: passwordDialog.active = false }
            function ask(id, wrong) {
                active = true
                item.ask(id, wrong)
            }
            function edit(id) {
                active = true
                item.edit(id)
            }
        }

        Toast {
            id: toast
            bottomOffset: win.mobile ? mobileNav.height + (mobilePlayer.visible ? 72 : 12) : playerBar.height + 22
        }

        WindowResizer {
            anchors.fill: parent
            enabled: !win.maximized && !win.mobile
        }

        // Phone layout: the pages that have no place in the bottom bar.
        Drawer {
            id: moreSheet
            edge: Qt.BottomEdge
            width: parent.width
            height: moreColumn.height + 24
            background: Rectangle {
                color: Theme.surface
                radius: 18
                Rectangle { anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: 18; color: parent.color }
            }

            Column {
                id: moreColumn
                y: 12
                width: parent.width
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 36; height: 4; radius: 2
                    color: Theme.alpha(Theme.text, 0.2)
                }
                Item { width: 1; height: 10 }
                Repeater {
                    model: [
                        { key: "sources", label: qsTr("Sources"), icon: "sources" },
                        { key: "equalizer", label: qsTr("Equalizer"), icon: "equalizer" },
                        { key: "stats", label: qsTr("Stats"), icon: "chart" },
                        { key: "settings", label: qsTr("Settings"), icon: "tune" }
                    ]
                    Item {
                        id: entry
                        required property var modelData
                        width: moreColumn.width
                        height: 54
                        Rectangle {
                            anchors.fill: parent
                            anchors.leftMargin: 10; anchors.rightMargin: 10
                            radius: 12
                            color: Nav.page === entry.modelData.key ? Theme.alpha(Theme.accent, 0.14)
                                                                    : entryMouse.pressed ? Theme.alpha(Theme.text, 0.06) : "transparent"
                        }
                        Icon {
                            x: 28
                            anchors.verticalCenter: parent.verticalCenter
                            name: entry.modelData.icon
                            size: 22
                            color: Nav.page === entry.modelData.key ? Theme.accent : Theme.textDim
                        }
                        Text {
                            x: 68
                            anchors.verticalCenter: parent.verticalCenter
                            text: entry.modelData.label
                            color: Theme.text
                            font.pixelSize: 16
                        }
                        MouseArea {
                            id: entryMouse
                            anchors.fill: parent
                            onClicked: { moreSheet.close(); Nav.navigate(entry.modelData.key) }
                        }
                    }
                }
            }
        }
    }

    Loader {
        readonly property var argv: Qt.application.arguments
        readonly property int at: argv.indexOf("--tour")
        active: at >= 0 && at + 1 < argv.length
        sourceComponent: DevTour {
            target: shell
            dir: parent.argv[parent.at + 1]
        }
    }

    Component { id: homePage; HomePage {} }
    Component { id: searchPage; SearchPage {} }
    Component { id: libraryPage; LibraryPage {} }
    Component { id: playlistPage; PlaylistPage {} }
    Component { id: artistPage; ArtistPage {} }
    Component { id: sourcesPage; SourcesPage {} }
    Component { id: settingsPage; SettingsPage {} }
    Component { id: equalizerPage; EqualizerPage {} }
    Component { id: statsPage; StatsPage {} }
    Component { id: roomPage; RoomPage {} }

    function pageComponent(name) {
        switch (name) {
        case "search": return searchPage
        case "library": return libraryPage
        case "playlist": return playlistPage
        case "artist": return artistPage
        case "sources": return sourcesPage
        case "settings": return settingsPage
        case "equalizer": return equalizerPage
        case "stats": return statsPage
        case "room": return roomPage
        default: return homePage
        }
    }

    Connections {
        target: Nav
        function onPageRequested(page, args) {
            stack.replace(null, win.pageComponent(page), { args: args })
            Memory.trimSoon()   // the previous page is gone once the transition ends
        }
        function onImmersiveChanged() {
            if (!Nav.immersive)
                Memory.trimSoon()
        }
        function onOnboardingChanged() {
            if (!Nav.onboarding)
                Memory.trimSoon()
        }
        function onToastRequested(text, icon) {
            toast.show(text, icon)
        }
        function onAddFolderRequested() {
            folderDialog.open()
        }
        function onMatchPickerRequested(track) {
            matchDialog.show(track)
        }
        function onPasswordDialogRequested(playlistId) { passwordDialog.edit(playlistId) }
        function onPlaylistDialogRequested(mode, index, rows) {
            playlistDialog.show(mode, index, rows)
        }
        function onWebLoginRequested(service) {
            webLogin.service = service
            webLogin.active = true
        }
        function onMoreRequested(open) {
            open ? moreSheet.open() : moreSheet.close()
        }
    }

    Connections {
        target: Playlists
        function onNotice(text, icon) { toast.show(text, icon) }
        function onCreated(kind, id) { Nav.openPlaylist(kind, id) }
    }

    Connections {
        target: Library
        function onNotice(text, icon) { toast.show(text, icon) }
        function onLibraryChanged() { Memory.trimSoon(3000) }
    }

    Connections {
        target: Player
        function onNotice(text, icon) { toast.show(text, icon) }
    }

    Connections {
        target: YouTube
        function onNotice(text, icon) { toast.show(text, icon) }
    }

    Connections {
        target: Spotify
        function onNotice(text, icon) { toast.show(text, icon) }
    }

    Connections {
        target: SoundCloud
        function onNotice(text, icon) { toast.show(text, icon) }
    }

    Connections {
        target: Lyrics
        function onNotice(text, icon) { toast.show(text, icon) }
    }

    Connections {
        target: Online
        function onNotice(text, icon) { toast.show(text, icon) }
        function onRoomOpened() { Nav.navigate("room") }
        function onPlaylistOpened(id) { Nav.openPlaylist("shared", id) }
        function onPasswordNeeded(id, wrong) { passwordDialog.ask(id, wrong) }
        // A nokturn:// link opened from a browser or a chat app
        function onActivateRequested() {
            if (win.visibility === Window.Minimized || win.visibility === Window.Hidden)
                win.showNormal()
            win.raise()
            win.requestActivate()
        }
    }

    Connections {
        target: Updater
        function onNotice(text, icon) { toast.show(text, icon) }
    }

    // The in-game overlay (GameOverlay): its window is made when the menu first opens.
    Loader {
        id: gameOverlay
        active: false
        sourceComponent: OverlayWindow {}
    }
    Connections {
        target: GameOverlay
        function onOpenChanged() { if (GameOverlay.open) gameOverlay.active = true }
    }

    // In-game lyrics HUD overlay: made when first opened.
    Loader {
        id: lyricsOverlay
        active: false
        sourceComponent: LyricsOverlayWindow {}
    }
    Connections {
        target: GameOverlay
        function onLyricsOpenChanged() { if (GameOverlay.lyricsOpen) lyricsOverlay.active = true }
    }

    FolderDialog {
        id: folderDialog
        title: qsTr("Choose a music folder")
        onAccepted: Library.addFolder(selectedFolder)
    }

    Shortcut { sequence: "Space"; onActivated: Player.toggle() }
    Shortcut { sequence: "Ctrl+Right"; onActivated: Player.next() }
    Shortcut { sequence: "Ctrl+Left"; onActivated: Player.previous() }
    Shortcut { sequence: "Ctrl+F"; onActivated: Nav.openSearch() }
    Shortcut { sequence: "Alt+Left"; onActivated: Nav.back() }
    Shortcut { sequence: "Alt+Right"; onActivated: Nav.forward() }
    Shortcut { sequence: "F11"; onActivated: Nav.immersive = !Nav.immersive }
    Shortcut { sequence: "Esc"; enabled: Nav.immersive; onActivated: Nav.immersive = false }
}
