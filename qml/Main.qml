import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import Nokturn

ApplicationWindow {
    id: win
    width: 1420
    height: 900
    minimumWidth: 1120
    minimumHeight: 720
    visible: true
    title: "Nokturn"
    color: Theme.bg
    flags: Qt.Window | Qt.FramelessWindowHint

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

        AmbientBackground {
            anchors.fill: parent
            visible: Theme.ambient
        }

        TitleBar {
            id: titleBar
            anchors { top: parent.top; left: parent.left; right: parent.right }
            height: 56
        }

        Sidebar {
            id: sidebar
            anchors {
                top: titleBar.bottom; left: parent.left; bottom: playerBar.top
                leftMargin: 10; bottomMargin: 2
            }
            width: Nav.sidebarCompact ? 76 : 252
            Behavior on width { NumberAnimation { duration: 460; easing.type: Easing.OutQuint } }
        }

        Panel {
            id: content
            anchors {
                top: titleBar.bottom; left: sidebar.right; right: queuePanel.left; bottom: playerBar.top
                leftMargin: 10; rightMargin: 10; bottomMargin: 2
            }

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
            width: Nav.queueOpen ? 344 : 0
            Behavior on width { NumberAnimation { duration: 520; easing.type: Easing.OutQuint } }
            Behavior on anchors.rightMargin { NumberAnimation { duration: 520; easing.type: Easing.OutQuint } }
        }

        PlayerBar {
            id: playerBar
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: 104
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

        Toast {
            id: toast
            bottomOffset: playerBar.height + 22
        }

        WindowResizer {
            anchors.fill: parent
            enabled: !win.maximized
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
        function onPlaylistDialogRequested(mode, index, rows) {
            playlistDialog.show(mode, index, rows)
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
        target: Lyrics
        function onNotice(text, icon) { toast.show(text, icon) }
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
