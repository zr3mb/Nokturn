import QtQuick
import QtQuick.Layouts
import Nokturn

Item {
    id: page
    property var args: ({})
    property string query: ""
    property int filter: 0
    readonly property var filterKeys: ["", "local", "youtube", "radio"]
    readonly property bool searching: query.trim().length > 0 || filter > 0
    readonly property var results: {
        if (Library.tracks.revision < 0)   // never: the read makes the binding re-run (a bare read is compiled away)
            return []
        return Library.search(query, filterKeys[filter])
    }
    // Online search in the YouTube Music catalog for "All" and "YouTube Music"
    readonly property bool onlineEnabled: query.trim().length > 1 && (filter === 0 || filter === 2)
    readonly property var online: {
        if (Library.tracks.revision < 0 || YouTube.resultsRevision < 0)
            return []
        return onlineEnabled ? Library.onlineResults() : []
    }

    // Public playlists for the query (YouTube Music's own, then users'); they open like an artist's albums.
    readonly property var playlists: onlineEnabled ? YouTube.playlistResults : []
    function openPlaylist(p) {
        Nav.navigate("playlist", { kind: "ytcollection", playlistId: p.id, title: p.title, subtitle: p.subtitle,
                                   cover: p.cover, label: qsTr("PLAYLIST") })
    }
    CardShadow { id: playlistShadow; size: 168 }

    // Rows are built at once (Repeater), so long result lists show in parts.
    property int shown: 40
    onQueryChanged: { shown = 40; onlineTimer.restart(); rememberState() }
    onFilterChanged: { shown = 40; onlineTimer.restart(); rememberState() }
    function rememberState() {
        if (Nav.page === "search")
            Nav.remember({ query: query, filter: filter })
    }

    Timer {
        id: onlineTimer
        interval: 450
        onTriggered: if (page.onlineEnabled) YouTube.search(page.query)
    }

    // Moods to search for when the library has (almost) no genre tags.
    readonly property var moods: [
        { name: "Ambient",     a: "#9fb5a8", b: "#36504a", seed: 0 },
        { name: "Lo-fi",       a: "#b7aac9", b: "#463c5c", seed: 1 },
        { name: qsTr("Night jazz"),  a: "#cdb68e", b: "#5a4a34", seed: 2 },
        { name: qsTr("Electronic"),  a: "#93a8c3", b: "#36445c", seed: 3 },
        { name: "Indie",       a: "#cda2a6", b: "#5c3a42", seed: 1 },
        { name: qsTr("Classical"),   a: "#c5bd95", b: "#545035", seed: 2 },
        { name: qsTr("Focus"),       a: "#8fb7b3", b: "#2e5150", seed: 0 },
        { name: qsTr("Rainy"),       a: "#a3b3c1", b: "#3b4a57", seed: 3 }
    ]
    // The genres in the user's library (tags, or the artist's genre for streaming tracks), the most tracks first.
    readonly property var genres: {
        if (Library.tracks.revision < 0 || Library.genresRevision < 0)
            return []
        return Library.genres()
    }
    property bool allGenres: false
    readonly property int genreLimit: 12
    readonly property var tiles: {
        const own = genres.slice(0, allGenres ? genres.length : genreLimit).map(g => ({
            name: g.name, key: g.key, count: g.count, a: g.colorA, b: g.colorB, seed: g.seed, cover: g.cover
        }))
        if (own.length >= 4)
            return own
        // A few genres only: moods to search for fill the rest of the grid.
        const names = own.map(t => t.name.toLowerCase())
        return own.concat(moods.filter(m => names.indexOf(m.name.toLowerCase()) < 0).slice(0, 8 - own.length))
    }

    Connections {
        target: Nav
        function onSearchFocusRequested() { field.forceActiveFocus() }
    }
    Component.onCompleted: {
        if (args && args.filter !== undefined)
            filter = args.filter
        if (args && args.query)
            field.text = args.query
        field.forceActiveFocus()
    }

    // A genre from the library (opens its tracks) or a mood (searches for it).
    component MoodTile: Item {
        id: tile
        required property var modelData
        required property int index
        readonly property bool isGenre: modelData.key !== undefined
        signal searchRequested(string text)
        implicitHeight: 124
        opacity: 0
        scale: 0.94

        SequentialAnimation {
            id: tileIn
            PauseAnimation { duration: 60 + tile.index * 45 }
            ParallelAnimation {
                NumberAnimation { target: tile; property: "opacity"; to: 1; duration: 420 }
                NumberAnimation { target: tile; property: "scale"; to: 1; duration: 560; easing.type: Easing.OutBack }
            }
        }
        Component.onCompleted: tileIn.start()

        HoverHandler { id: tileHover; cursorShape: Qt.PointingHandCursor }
        TapHandler {
            onTapped: {
                if (tile.isGenre)
                    Nav.navigate("playlist", { kind: "genre", key: tile.modelData.key, name: tile.modelData.name })
                else
                    tile.searchRequested(tile.modelData.name)
            }
        }

        Item {
            anchors.fill: parent
            scale: tileHover.hovered ? 1.03 : 1
            Behavior on scale { NumberAnimation { duration: 380; easing.type: Easing.OutCubic } }

            ArtCover {
                anchors.fill: parent
                radius: 18
                colorA: tile.modelData.a
                colorB: tile.modelData.b
                seed: tile.modelData.seed
                decorated: !cornerCover.visible
            }
            // The cover of the genre's biggest album, tilted in the corner
            ArtCover {
                id: cornerCover
                visible: (tile.modelData.cover || "") !== ""
                anchors { right: parent.right; bottom: parent.bottom; rightMargin: 14; bottomMargin: 14 }
                width: 66; height: 66
                radius: 8
                colorA: tile.modelData.a
                colorB: tile.modelData.b
                seed: tile.modelData.seed
                source: tile.modelData.cover || ""
                rotation: tileHover.hovered ? 6 : 14
                Behavior on rotation { NumberAnimation { duration: 420; easing.type: Easing.OutBack } }
            }
            Column {
                anchors { left: parent.left; right: cornerCover.visible ? cornerCover.left : parent.right; bottom: parent.bottom
                          leftMargin: 18; rightMargin: 12; bottomMargin: 16 }
                spacing: 2
                Text {
                    width: parent.width
                    text: tile.modelData.name
                    color: "#f6f2ec"
                    font.family: Theme.fontDisplay
                    font.pixelSize: 21
                    font.weight: Font.Bold
                    fontSizeMode: Text.HorizontalFit   // long names shrink a little before they're cut
                    minimumPixelSize: 15
                    elide: Text.ElideRight
                }
                Text {
                    visible: tile.isGenre
                    text: qsTr("%n tracks", "", tile.modelData.count || 0)
                    color: Qt.rgba(0.965, 0.95, 0.925, 0.72)
                    font.pixelSize: 13
                }
            }
        }
    }

    PageFlick {
        anchors.fill: parent

        Reveal {
            Text {
                text: qsTr("Search")
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
                spacing: 18

                InputField {
                    id: field
                    Layout.fillWidth: true
                    implicitHeight: 56
                    icon: "search"
                    font.pixelSize: 16
                    placeholderText: qsTr("What do you want to listen to today?")
                    onTextChanged: page.query = text
                    Keys.onEscapePressed: text = ""
                }

                SegmentedTabs {
                    model: [qsTr("All"), qsTr("Local"), "YouTube Music", qsTr("Radio")]
                    currentIndex: page.filter
                    onActivated: i => page.filter = i
                }
            }
        }

        Loader {
            id: body
            Layout.fillWidth: true
            Layout.preferredHeight: item ? item.implicitHeight : 0
            sourceComponent: page.searching ? resultsView : browseView
            onLoaded: fade.restart()
            NumberAnimation { id: fade; target: body; property: "opacity"; from: 0; to: 1; duration: 320; easing.type: Easing.OutCubic }
        }
    }

    Component {
        id: resultsView

        ColumnLayout {
            spacing: 6

            Text {
                Layout.bottomMargin: 8
                visible: page.results.length > 0
                text: qsTr("In the library: %n results", "", page.results.length)
                      + (page.query.trim() ? " " + qsTr("for “%1”").arg(page.query.trim()) : "")
                color: Theme.textDim
                font.pixelSize: 14
            }

            TrackListHeader { visible: page.results.length > 0 }

            Repeater {
                model: page.results.slice(0, page.shown)
                TrackRow {
                    required property var modelData
                    required property int index
                    Layout.fillWidth: true
                    trackIndex: modelData
                    number: index + 1
                    queue: page.results
                    revealDelay: Math.min(index, 12) * 32
                }
            }

            PillButton {
                visible: page.results.length > page.shown
                Layout.topMargin: 6
                kind: "ghost"
                icon: "expandMore"
                text: qsTr("Show all (%1)").arg(page.results.length)
                onClicked: page.shown = page.results.length
            }

            // Public playlists on YouTube Music
            RowLayout {
                visible: page.onlineEnabled && page.playlists.length > 0
                Layout.fillWidth: true
                Layout.topMargin: page.results.length > 0 ? 26 : 0
                Layout.bottomMargin: 8
                spacing: 10
                Icon { name: "library"; size: 20; color: "#cf9d9d" }
                Text {
                    Layout.fillWidth: true
                    text: qsTr("Playlists on YouTube Music")
                    color: Theme.text
                    font.family: Theme.fontDisplay
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                }
            }
            Flow {
                visible: page.onlineEnabled && page.playlists.length > 0
                Layout.fillWidth: true
                spacing: 18
                Repeater {
                    model: page.onlineEnabled ? page.playlists : []
                    AlbumCard {
                        required property var modelData
                        required property int index
                        width: 168
                        shadow: playlistShadow
                        title: modelData.title
                        subtitle: modelData.subtitle
                        cover: modelData.cover
                        seed: index
                        revealDelay: index * 30
                        onClicked: page.openPlaylist(modelData)
                        onPlayClicked: page.openPlaylist(modelData)
                    }
                }
            }

            RowLayout {
                visible: page.onlineEnabled && (page.online.length > 0 || YouTube.searching)
                Layout.fillWidth: true
                Layout.topMargin: page.results.length > 0 || page.playlists.length > 0 ? 26 : 0
                Layout.bottomMargin: 8
                spacing: 10
                Icon { name: "playCircle"; size: 20; color: "#cf9d9d" }
                Text {
                    Layout.fillWidth: true
                    text: qsTr("On YouTube Music")
                    color: Theme.text
                    font.family: Theme.fontDisplay
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                }
                Spinner { visible: YouTube.searching; size: 18; color: Theme.textDim }
            }

            Repeater {
                model: page.online
                TrackRow {
                    required property var modelData
                    required property int index
                    Layout.fillWidth: true
                    trackIndex: modelData
                    number: index + 1
                    queue: page.online
                    revealDelay: Math.min(index, 12) * 32
                }
            }

            ColumnLayout {
                visible: page.results.length === 0 && page.online.length === 0 && page.playlists.length === 0 && !YouTube.searching
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 50
                spacing: 10

                Icon {
                    Layout.alignment: Qt.AlignHCenter
                    name: "search"
                    size: 52
                    color: Theme.textFaint
                }
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("No results for “%1”").arg(page.query.trim())
                    color: Theme.text
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                }
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Try a different phrase or connect another music source.")
                    color: Theme.textDim
                    font.pixelSize: 14
                }
            }
        }
    }

    Component {
        id: browseView

        ColumnLayout {
            spacing: 18

            SectionHeader {
                Layout.fillWidth: true
                title: page.genres.length > 0 ? qsTr("Your genres") : qsTr("Browse moods")
                subtitle: page.genres.length > 0 ? qsTr("What your library is made of — or just start typing")
                                                 : qsTr("Or just start typing")
            }

            Grid {
                id: moodGrid
                Layout.fillWidth: true
                Layout.preferredWidth: 100
                columns: Math.max(2, Math.floor((width + columnSpacing) / 230))
                columnSpacing: 16
                rowSpacing: 16
                readonly property real cellW: (width - columnSpacing * (columns - 1)) / columns

                Repeater {
                    model: page.tiles
                    MoodTile {
                        width: moodGrid.cellW
                        onSearchRequested: text => field.text = text
                    }
                }
            }

            PillButton {
                visible: page.genres.length > page.genreLimit
                kind: "ghost"
                icon: page.allGenres ? "" : "expandMore"
                text: page.allGenres ? qsTr("Show less") : qsTr("Show all (%1)").arg(page.genres.length)
                fontSize: 13
                onClicked: page.allGenres = !page.allGenres
            }
        }
    }
}
