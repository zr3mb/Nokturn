import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import Nokturn

// Playlist, album or artist view. The list is virtualized (ListView), because YouTube Music
// playlists can have thousands of tracks.
Item {
    id: page
    property var args: ({})

    readonly property var info: {
        const a = args || {}
        // Never true: the reads make the binding re-run when they change (a bare read is compiled away).
        // A collection's tracks are appended to the model, not rebuilt into it; genres fill in later.
        if (Library.tracks.revision < 0 || (a.kind === "ytcollection" && YouTube.resultsRevision < 0)
                || (a.kind === "genre" && Library.genresRevision < 0))
            return {}
        if (a.kind === "album") {
            const al = Library.albums.find(x => x.key === a.key) || { title: "", artist: "", year: 0 }
            return { label: qsTr("ALBUM"), title: al.title, meta: [al.artist].concat(al.year > 0 ? [String(al.year)] : []),
                     colorA: al.colorA || Theme.accent, colorB: al.colorB || Theme.accent2, seed: al.seed || 0,
                     cover: al.cover || "", icon: "", round: false,
                     rows: Library.tracksOfAlbum(a.key) }
        }
        if (a.kind === "artist") {
            const ar = Library.artists.find(x => x.name === a.artist) || {}
            return { label: qsTr("ARTIST"), title: a.artist, meta: [],
                     colorA: ar.colorA || Theme.accent, colorB: ar.colorB || Theme.accent2, seed: ar.seed || 0,
                     cover: ar.cover || "", icon: "", round: true,
                     rows: Library.tracksOfArtist(a.artist) }
        }
        if (a.kind === "genre") {
            // A genre of the library (the search page): tags, or the artists' genres for streaming tracks
            const g = Library.genres().find(x => x.key === a.key) || { name: a.name || "", seed: 0, cover: "", mosaic: [] }
            return { label: qsTr("GENRE"), title: g.name, meta: [qsTr("From your library")],
                     colorA: g.colorA || Theme.accent, colorB: g.colorB || Theme.accent2, seed: g.seed,
                     cover: g.mosaic.length >= 4 ? "" : g.cover, mosaic: g.mosaic, icon: "", round: false,
                     rows: Library.tracksOfGenre(a.key) }
        }
        if (a.kind === "ytcollection") {
            // An album / playlist opened from an artist profile — loaded from YouTube Music, not in the library.
            return { label: a.label || qsTr("ALBUM"), title: a.title || "", meta: a.subtitle ? [a.subtitle] : [],
                     colorA: Theme.accent, colorB: Theme.accent2, seed: 0, cover: a.cover || "", icon: "",
                     round: a.round === true, rows: Library.collectionRows(a.playlistId),
                     loading: YouTube.collectionLoading(a.playlistId) }
        }
        const index = a.index || 0
        const p = Library.playlists[index]
        if (!p)
            return { label: qsTr("PLAYLIST"), title: "", meta: [], colorA: Theme.accent, colorB: Theme.accent2,
                     seed: 0, cover: "", icon: "music", round: false, rows: [], loading: false }
        const yt = p.kind === "ytplaylist"
        const sp = p.kind === "spplaylist" || p.kind === "spotifyliked"
        const meta = yt ? "YouTube Music"
                   : p.kind === "youtube" ? (YouTube.connected ? qsTr("Liked on your YouTube account") : qsTr("Sample tracks from YouTube Music"))
                   : p.kind === "spotifyliked" ? qsTr("Liked on your Spotify account")
                   : sp ? qsTr("Spotify — played from YouTube Music")
                   : p.kind === "user" ? (p.description || qsTr("Your playlist on this computer"))
                   : qsTr("Your playlist")
        return { label: yt ? qsTr("PLAYLIST · YOUTUBE MUSIC") : sp ? qsTr("PLAYLIST · SPOTIFY") : qsTr("PLAYLIST"), title: p.name, meta: [meta],
                 colorA: p.colorA, colorB: p.colorB, seed: p.seed, cover: p.cover || "", icon: p.icon, round: false,
                 mosaic: p.mosaic || [], playlistIndex: index, customCover: p.customCover === true,
                 kind: p.kind, editable: Playlists.canEdit(index),
                 rows: Library.playlistRows(index), loading: Library.playlistLoading(index) }
    }
    // Queue id for the mix settings (separate for each playlist / album / artist).
    readonly property string context: {
        const a = args || {}
        if (a.kind === "album") return "album:" + a.key
        if (a.kind === "artist") return "artist:" + a.artist
        if (a.kind === "ytcollection") return "ytc:" + a.playlistId
        if (a.kind === "genre") return "genre:" + a.key
        const p = Library.playlists[a.index || 0]
        if (!p) return ""
        return p.kind === "ytplaylist" ? "yt:" + p.ytId : p.kind === "spplaylist" ? "sp:" + p.spId
             : p.kind === "user" ? "user:" + p.userId : "playlist:" + p.kind
    }

    // Sort order of this list — remembered per playlist / album ("" = as the source orders it).
    property string sortKey: Prefs.stringValue("playlist/sort/" + context, "")
    onContextChanged: sortKey = Prefs.stringValue("playlist/sort/" + context, "")
    function setSort(key) {
        sortKey = key
        Prefs.setValue("playlist/sort/" + context, key)
    }
    readonly property var rows: {
        const rows = info.rows
        if (sortKey === "")
            return rows
        if (sortKey === "reverse")
            return rows.slice().reverse()
        const field = sortKey === "duration" ? "duration" : sortKey
        const items = rows.map(r => ({ r: r, v: Library.tracks.get(r)[field] }))
        items.sort((a, b) => field === "duration" ? a.v - b.v
                                                  : String(a.v).localeCompare(String(b.v), undefined, { sensitivity: "base" }))
        return items.map(x => x.r)
    }
    readonly property var mix: Player.mixRevision >= 0 ? Player.mixFor(context) : ({})
    readonly property int totalSeconds: Library.totalDuration(info.rows)
    readonly property real pad: 34

    readonly property bool playingHere: Player.playing && info.rows.indexOf(Player.currentIndex) >= 0

    // Colored glow behind the header (with a slight parallax)
    Rectangle {
        width: parent.width
        height: 420
        readonly property real scrolled: flick.contentY - flick.originY
        y: -scrolled * 0.45
        opacity: Math.max(0, 1 - scrolled / 420)
        gradient: Gradient {
            GradientStop { position: 0; color: Theme.alpha(page.info.colorA, 0.42) }
            GradientStop { position: 1; color: Theme.alpha(page.info.colorA, 0) }
        }
    }

    ListView {
        id: flick
        anchors.fill: parent
        clip: true
        leftMargin: page.pad
        rightMargin: page.pad
        bottomMargin: page.pad
        cacheBuffer: 800
        boundsBehavior: Flickable.StopAtBounds
        flickDeceleration: 3200
        maximumFlickVelocity: 4200
        ScrollBar.vertical: ThinScrollBar {}
        model: page.rows

        header: Item {
            width: flick.width - page.pad * 2
            height: headerColumn.implicitHeight + page.pad + 12

            ColumnLayout {
                id: headerColumn
                y: page.pad
                width: parent.width
                spacing: 34

                Reveal {
                    RowLayout {
                        width: parent.width
                        spacing: 30

                        ArtCover {
                            Layout.preferredWidth: 224
                            Layout.preferredHeight: 224
                            radius: page.info.round ? 112 : 20
                            colorA: page.info.colorA
                            colorB: page.info.colorB
                            seed: page.info.seed
                            source: page.info.cover
                            mosaic: page.info.mosaic || []
                            decorated: page.info.icon === ""
                            layer.enabled: true
                            layer.effect: MultiEffect {
                                shadowEnabled: true
                                shadowBlur: 1.0
                                shadowOpacity: 0.6
                                shadowVerticalOffset: 18
                            }
                            Icon {
                                anchors.centerIn: parent
                                visible: page.info.icon !== "" && !parent.hasPicture
                                name: page.info.icon
                                size: 84
                                color: "#f4efe8"
                            }

                            // Playlists: your own picture or a different collage
                            HoverHandler { id: coverHover; enabled: page.info.playlistIndex !== undefined }
                            Rectangle {
                                id: coverButton
                                anchors { right: parent.right; bottom: parent.bottom; margins: 12 }
                                width: 40; height: 40
                                radius: 20
                                visible: page.info.playlistIndex !== undefined
                                opacity: coverHover.hovered || coverMenu.opened ? 1 : 0
                                Behavior on opacity { NumberAnimation { duration: Theme.fast } }
                                color: Qt.rgba(0, 0, 0, editArea.containsMouse ? 0.72 : 0.55)
                                Icon {
                                    anchors.centerIn: parent
                                    name: "edit"
                                    size: 18
                                    color: "#f4efe8"
                                }
                                MouseArea {
                                    id: editArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        const p = coverButton.mapToItem(page, 0, coverButton.height + 8)
                                        coverMenu.x = p.x
                                        coverMenu.y = p.y
                                        coverMenu.open()
                                    }
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignBottom
                            spacing: 8

                            Text {
                                text: page.info.label
                                color: Theme.textDim
                                font.pixelSize: 12
                                font.weight: Font.Bold
                                font.letterSpacing: 2.4
                            }
                            Text {
                                Layout.fillWidth: true
                                text: page.info.title
                                color: Theme.text
                                font.family: Theme.fontDisplay
                                font.pixelSize: page.info.title.length > 16 ? 48 : 64
                                font.weight: Font.Bold
                                wrapMode: Text.WordWrap
                                maximumLineCount: 2
                                elide: Text.ElideRight
                                lineHeight: 0.95
                            }
                            Text {
                                Layout.fillWidth: true
                                text: page.info.meta.concat([
                                          qsTr("%n tracks", "", page.info.rows.length),
                                          Theme.formatTotal(page.totalSeconds)
                                      ]).join("  ·  ")
                                color: Theme.textDim
                                font.pixelSize: 14
                                elide: Text.ElideRight
                            }
                        }
                    }
                }

                Reveal {
                    delay: 80
                    RowLayout {
                        width: parent.width
                        spacing: 12

                        PlayFab {
                            size: 58
                            playing: page.playingHere
                            enabled: page.info.rows.length > 0
                            onClicked: page.playingHere ? Player.toggle() : Player.playFrom(page.rows, page.rows[0], page.context)
                        }
                        IconButton {
                            icon: "shuffle"
                            iconSize: 24
                            implicitWidth: 46; implicitHeight: 46
                            active: Player.shuffle
                            onClicked: {
                                Player.shuffle = true
                                const rows = page.rows
                                if (rows.length) Player.playFrom(rows, rows[Math.floor(Math.random() * rows.length)], page.context)
                            }
                        }
                        // "Mix" like in Spotify: smooth, DJ-style transitions between this playlist's tracks.
                        PillButton {
                            visible: page.context !== ""
                            kind: page.mix.enabled ? "primary" : "ghost"
                            icon: "mix"
                            text: page.mix.enabled ? qsTr("Mix") + " · " + Player.mixStyleName(page.mix.style) : qsTr("Mix")
                            fontSize: 13
                            implicitHeight: 40
                            onClicked: {
                                Player.setMix(page.context, !page.mix.enabled, page.mix.style, page.mix.length)
                                Nav.toast(page.mix.enabled ? qsTr("Mixing on — tracks will flow smoothly")
                                                           : qsTr("Mixing off"), "mix")
                            }
                        }
                        IconButton {
                            visible: page.context !== "" && page.mix.enabled
                            icon: "tune"
                            iconSize: 22
                            implicitWidth: 42; implicitHeight: 42
                            showDot: false
                            onClicked: mixPopup.open()
                        }
                        IconButton {
                            id: moreButton
                            icon: "more"
                            iconSize: 24
                            implicitWidth: 46; implicitHeight: 46
                            showDot: false
                            onClicked: {
                                const p = moreButton.mapToItem(page, 0, moreButton.height + 4)
                                playlistMenu.x = Math.min(p.x, page.width - playlistMenu.width - 12)
                                playlistMenu.y = p.y
                                playlistMenu.confirmDelete = false
                                playlistMenu.open()
                            }
                        }
                        Item { Layout.fillWidth: true }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    TrackListHeader {
                        visible: page.info.rows.length > 0
                        showAlbum: page.info.label !== "ALBUM"
                    }

                    ColumnLayout {
                        visible: page.info.rows.length === 0
                        Layout.alignment: Qt.AlignHCenter
                        Layout.topMargin: 40
                        spacing: 10
                        Icon {
                            Layout.alignment: Qt.AlignHCenter
                            name: page.info.loading ? "refresh" : "heartOutline"
                            size: 48
                            color: Theme.textFaint
                        }
                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: page.info.loading ? qsTr("Loading the playlist…") : qsTr("Nothing here yet")
                            color: Theme.text
                            font.pixelSize: 18
                            font.weight: Font.DemiBold
                        }
                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: page.info.loading ? qsTr("Fetching tracks from YouTube Music")
                                  : page.info.editable === true ? qsTr("Add tracks with ⋯ → Add to playlist on any track.")
                                  : qsTr("Like a track with the heart and it will show up here.")
                            color: Theme.textDim
                            font.pixelSize: 14
                        }
                    }
                }
            }
        }

        delegate: TrackRow {
            required property var modelData
            required property int index
            width: flick.width - page.pad * 2
            trackIndex: modelData
            number: index + 1
            showAlbum: page.info.label !== "ALBUM"
            queue: page.rows
            context: page.context
            playlistIndex: page.info.editable === true ? page.info.playlistIndex : -1
            revealDelay: index < 16 ? 180 + index * 30 : 0
        }
    }

    // Small pieces of the "⋯" menu
    component MenuLabel: Text {
        Layout.fillWidth: true
        Layout.leftMargin: 12
        Layout.topMargin: 8
        Layout.bottomMargin: 2
        color: Theme.textFaint
        font.pixelSize: 11
        font.weight: Font.DemiBold
        font.letterSpacing: 1
    }
    component Separator: Rectangle {
        Layout.fillWidth: true
        Layout.topMargin: 4
        Layout.bottomMargin: 4
        height: 1
        color: Qt.rgba(1, 1, 1, 0.07)
    }

    // "⋯" next to Play: sort, save as a new playlist, and for the user's own playlists editing,
    // who can see it (YouTube Music) and deleting.
    Popup {
        id: playlistMenu
        property bool confirmDelete: false
        readonly property bool own: page.info.editable === true
        width: 300
        padding: 6
        margins: 10
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle {
            radius: 14
            color: Theme.surfaceHi
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.07)
        }
        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 140 }
            NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: 220; easing.type: Easing.OutCubic }
        }
        exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 120 } }

        contentItem: ColumnLayout {
            spacing: 2

            MenuEntry {
                visible: playlistMenu.own
                Layout.fillWidth: true
                icon: "tune"
                text: page.info.kind === "ytplaylist" ? qsTr("Rename") : qsTr("Edit name and description")
                onTriggered: {
                    playlistMenu.close()
                    Nav.editPlaylist(page.info.playlistIndex)
                }
            }
            MenuEntry {
                Layout.fillWidth: true
                visible: page.rows.length > 0
                icon: "add"
                text: qsTr("Save as a new playlist…")
                onTriggered: {
                    playlistMenu.close()
                    Nav.createPlaylist(page.rows)
                }
            }

            // Who can see it — YouTube Music playlists
            MenuLabel {
                visible: playlistMenu.own && page.info.kind === "ytplaylist"
                text: qsTr("WHO CAN SEE IT")
            }
            Repeater {
                model: playlistMenu.own && page.info.kind === "ytplaylist"
                       ? [{ key: "PRIVATE", text: qsTr("Private"), icon: "person" },
                          { key: "UNLISTED", text: qsTr("Unlisted — anyone with the link"), icon: "link" },
                          { key: "PUBLIC", text: qsTr("Public"), icon: "sources" }] : []
                MenuEntry {
                    required property var modelData
                    Layout.fillWidth: true
                    icon: modelData.icon
                    text: modelData.text
                    onTriggered: {
                        playlistMenu.close()
                        Playlists.setPrivacy(page.info.playlistIndex, modelData.key)
                    }
                }
            }

            MenuLabel { text: qsTr("SORT BY") }
            Repeater {
                model: [{ key: "", text: qsTr("Playlist order") }, { key: "reverse", text: qsTr("Playlist order, reversed") },
                        { key: "title", text: qsTr("Title") }, { key: "artist", text: qsTr("Artist") },
                        { key: "album", text: qsTr("Album") }, { key: "duration", text: qsTr("Duration") }]
                MenuEntry {
                    required property var modelData
                    Layout.fillWidth: true
                    icon: page.sortKey === modelData.key ? "check" : ""
                    text: modelData.text
                    onTriggered: {
                        playlistMenu.close()
                        page.setSort(modelData.key)
                    }
                }
            }

            Separator { visible: playlistMenu.own }
            MenuEntry {
                visible: playlistMenu.own
                Layout.fillWidth: true
                icon: "close"
                text: playlistMenu.confirmDelete
                      ? (page.info.kind === "ytplaylist" ? qsTr("Click again — it's deleted from YouTube Music too")
                                                         : qsTr("Click again to delete"))
                      : qsTr("Delete playlist")
                onTriggered: {
                    if (!playlistMenu.confirmDelete) {
                        playlistMenu.confirmDelete = true
                        return
                    }
                    playlistMenu.close()
                    Playlists.deletePlaylist(page.info.playlistIndex)
                    Nav.navigate("home")
                }
            }
        }
    }

    // Playlist cover: upload a picture, shuffle the collage, go back to the default
    Popup {
        id: coverMenu
        width: 300
        padding: 6
        margins: 10
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle {
            radius: 14
            color: Theme.surfaceHi
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.07)
        }
        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 140 }
            NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: 220; easing.type: Easing.OutCubic }
        }
        exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 120 } }

        contentItem: ColumnLayout {
            spacing: 2
            MenuEntry {
                Layout.fillWidth: true
                icon: "image"
                text: qsTr("Upload your own picture…")
                onTriggered: {
                    coverMenu.close()
                    coverDialog.open()
                }
            }
            MenuEntry {
                Layout.fillWidth: true
                visible: !page.info.customCover && (page.info.mosaic || []).length >= 4
                icon: "shuffle"
                text: qsTr("Shuffle the collage")
                onTriggered: {
                    coverMenu.close()
                    Library.shufflePlaylistCover(page.info.playlistIndex)
                }
            }
            MenuEntry {
                Layout.fillWidth: true
                visible: page.info.customCover === true
                icon: "refresh"
                text: qsTr("Restore the default cover")
                onTriggered: {
                    coverMenu.close()
                    Library.resetPlaylistCover(page.info.playlistIndex)
                    Nav.toast(qsTr("The default cover is back"), "image")
                }
            }
        }
    }

    FileDialog {
        id: coverDialog
        title: qsTr("Choose a picture for the playlist")
        nameFilters: [qsTr("Images (*.jpg *.jpeg *.png *.webp *.bmp *.gif)")]
        onAccepted: {
            if (Library.setPlaylistCover(page.info.playlistIndex, selectedFile))
                Nav.toast(qsTr("New playlist cover"), "image")
        }
    }

    // Mix settings for this playlist
    Popup {
        id: mixPopup
        anchors.centerIn: parent
        width: Math.min(560, page.width - 60)
        modal: true
        focus: true
        padding: 26
        background: Rectangle {
            radius: 22
            color: Theme.surfaceHi
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.07)
        }
        Overlay.modal: Rectangle { color: "#88000000" }
        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 200 }
            NumberAnimation { property: "scale"; from: 0.95; to: 1; duration: 320; easing.type: Easing.OutBack }
        }
        exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 160 } }

        contentItem: ColumnLayout {
            spacing: 16
            RowLayout {
                Layout.fillWidth: true
                Icon { name: "mix"; size: 22; color: Theme.accent }
                Text {
                    Layout.fillWidth: true
                    text: qsTr("Mix: %1").arg(page.info.title)
                    color: Theme.text
                    font.family: Theme.fontDisplay
                    font.pixelSize: 20
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }
                IconButton { icon: "close"; showDot: false; onClicked: mixPopup.close() }
            }
            Text {
                Layout.fillWidth: true
                text: qsTr("Transition style")
                color: Theme.textDim
                font.pixelSize: 12
                font.weight: Font.Bold
                font.letterSpacing: 1.2
            }
            Flow {
                Layout.fillWidth: true
                spacing: 8
                Repeater {
                    model: Player.mixStyles
                    PillButton {
                        required property string modelData
                        kind: page.mix.style === modelData ? "primary" : "secondary"
                        text: Player.mixStyleName(modelData)
                        fontSize: 13
                        implicitHeight: 34
                        onClicked: Player.setMix(page.context, true, modelData, page.mix.length)
                    }
                }
            }
            Text {
                Layout.fillWidth: true
                text: Player.mixStyleDescription(page.mix.style)
                color: Theme.textDim
                font.pixelSize: 13
                wrapMode: Text.WordWrap
            }
            Text {
                Layout.fillWidth: true
                Layout.topMargin: 6
                text: qsTr("Transition length")
                color: Theme.textDim
                font.pixelSize: 12
                font.weight: Font.Bold
                font.letterSpacing: 1.2
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 14
                ProgressSlider {
                    Layout.fillWidth: true
                    value: (page.mix.length - 2) / 14
                    labelFor: function(v) { return Math.round(2 + v * 14) + " s" }
                    onMoved: v => Player.setMix(page.context, true, page.mix.style, Math.round(2 + v * 14))
                }
                Text {
                    text: page.mix.length + " s"
                    color: Theme.text
                    font.pixelSize: 14
                    font.features: { "tnum": 1 }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 4
                spacing: 12
                Text {
                    Layout.fillWidth: true
                    text: qsTr("Smart transitions (skipping silence, detecting fade-outs) — shared by all mixes.")
                    color: Theme.textFaint
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }
                Toggle { checked: Player.smartTransitions; onToggled: c => Player.smartTransitions = c }
            }
            PillButton {
                Layout.alignment: Qt.AlignRight
                kind: "ghost"
                text: qsTr("Turn off the mix")
                onClicked: {
                    Player.setMix(page.context, false, page.mix.style, page.mix.length)
                    mixPopup.close()
                }
            }
        }
    }
}