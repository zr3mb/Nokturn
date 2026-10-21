import QtQuick
import QtQuick.Effects
import QtQuick.Layouts
import Nokturn

Item {
    id: page
    property var args: ({})

    // The built-in lists, next to "Continue listening"; { p, index } with the Library.playlists index.
    readonly property var shortcuts: {
        const out = []
        Library.playlists.forEach((p, i) => {
            if (["liked", "recent", "history", "youtube"].includes(p.kind))
                out.push({ p: p, index: i })
        })
        return out.concat([
            { p: { name: qsTr("All tracks"), kind: "all", icon: "library", colorA: "#a3b3c1", colorB: "#3b4a57", seed: 0 }, index: -1 },
            { p: { name: qsTr("Random mix"), kind: "mix", icon: "shuffle", colorA: "#b7aac9", colorB: "#463c5c", seed: 0 }, index: -1 },
            { p: { name: qsTr("Night Jazz — live"), kind: "radio", icon: "live", colorA: "#b9a889", colorB: "#4a4232", seed: 0 }, index: -1 }
        ])
    }
    // Whole playlists (your own, YouTube Music, Spotify), at the end of the page.
    readonly property var fullPlaylists: {
        const out = []
        Library.playlists.forEach((p, i) => {
            if (!["liked", "recent", "history", "youtube"].includes(p.kind))
                out.push({ p: p, index: i })
        })
        return out
    }

    // Picked from YouTube Music radios of the tracks that say the most about the taste (Recommendations).
    readonly property var recommended: {
        // The picks are appended to the model, not rebuilt into it. (Never true: the reads make the binding re-run.)
        if (Library.tracks.revision < 0 || YouTube.resultsRevision < 0)
            return []
        return Library.recommendedRows()
    }
    property bool allRecommended: false

    function rowsOf(p, index) {
        if (p.kind === "radio") return Library.search("", "radio", 1)
        if (p.kind === "all" || p.kind === "mix") return Library.localTracks()
        return Library.playlistRows(index)
    }

    function playPick(p, index) {
        const rows = rowsOf(p, index)
        if (rows.length === 0) {
            Nav.toast(Library.localCount === 0 ? qsTr("Add a music folder first") : qsTr("This playlist is still empty"), "music")
            return
        }
        if (p.kind === "mix")
            Player.playShuffled(rows)
        else
            Player.playFrom(rows, rows[0])
    }

    function openPick(p, index) {
        if (p.kind === "all") Nav.navigate("library")
        else if (p.kind === "mix" || p.kind === "radio") playPick(p, index)
        else Nav.navigate("playlist", { kind: "playlist", index: index })
    }

    component SourceMini: Item {
        id: mini
        required property var modelData
        required property int index
        readonly property bool isLocal: modelData.id === "local"
        readonly property string status: modelData.id === "youtube"
                                         ? (YouTube.connected ? "connected" : YouTube.loggingIn ? "connecting" : "disconnected")
                                         : isLocal ? (Library.folders.length > 0 ? "connected" : "disconnected")
                                         : modelData.status

        Layout.fillWidth: true
        Layout.preferredWidth: 100
        implicitHeight: 84

        HoverHandler { id: miniHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: Nav.navigate("sources") }

        Rectangle {
            anchors.fill: parent
            radius: 16
            color: Theme.alpha(Theme.text, miniHover.hovered ? 0.075 : 0.045)
            border.width: 1
            border.color: Theme.alpha(mini.modelData.color, miniHover.hovered ? 0.3 : 0.08)
            Behavior on color { ColorAnimation { duration: Theme.normal } }
            Behavior on border.color { ColorAnimation { duration: Theme.normal } }
        }

        Rectangle {
            id: miniIcon
            x: 16
            anchors.verticalCenter: parent.verticalCenter
            width: 46; height: 46; radius: 14
            color: Theme.alpha(mini.modelData.color, 0.15)
            scale: miniHover.hovered ? 1.06 : 1
            rotation: miniHover.hovered ? -5 : 0
            Behavior on scale { NumberAnimation { duration: 320; easing.type: Easing.OutBack } }
            Behavior on rotation { NumberAnimation { duration: 320; easing.type: Easing.OutBack } }
            Icon {
                anchors.centerIn: parent
                name: mini.modelData.icon
                size: 22
                color: mini.modelData.color
            }
        }

        Column {
            anchors { left: miniIcon.right; leftMargin: 14; right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
            spacing: 4
            Text {
                width: parent.width
                text: mini.modelData.name
                color: Theme.text
                font.pixelSize: 15
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            Row {
                spacing: 6
                Text {
                    text: mini.isLocal
                          ? (Library.scanning ? qsTr("Scanning…")
                             : Library.folders.length > 0 ? qsTr("%n tracks", "", Library.localCount)
                             : qsTr("Add folder →"))
                          : mini.modelData.id === "youtube" && YouTube.connected
                            ? qsTr("%n liked", "", YouTube.likedCount)
                          : mini.status === "connected" ? qsTr("Connected")
                          : mini.status === "disconnected" ? qsTr("Sign in →") : mini.status === "connecting" ? qsTr("Signing in…") : qsTr("Ready to use")
                    color: Theme.textDim
                    font.pixelSize: 12
                }
            }
        }
    }

    // The shadow shared by the album cards
    CardShadow {
        id: albumShadow
        size: albumGrid.cellW
    }

    PageFlick {
        anchors.fill: parent

        // Cat theme: a neon banner above the greeting
        Reveal {
            visible: Theme.cat
            Rectangle {
                id: banner
                width: parent.width
                height: 168
                radius: 22
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: Theme.alpha(Theme.accent2, 0.5) }
                    GradientStop { position: 0.55; color: Theme.alpha(Theme.accent, 0.2) }
                    GradientStop { position: 1.0; color: Theme.alpha(Theme.accent, 0.06) }
                }
                border.width: 1
                border.color: Theme.alpha(Theme.accent, 0.3)

                Repeater {
                    model: [
                        { x: 0.62, y: 0.18, s: 40, i: "heartOutline", r: 12 },
                        { x: 0.74, y: 0.52, s: 26, i: "heart", r: -10 },
                        { x: 0.83, y: 0.14, s: 20, i: "sparkle", r: 0 },
                        { x: 0.90, y: 0.46, s: 52, i: "cat", r: 8 },
                        { x: 0.68, y: 0.74, s: 16, i: "sparkle", r: 0 }
                    ]
                    Icon {
                        required property var modelData
                        x: banner.width * modelData.x
                        y: banner.height * modelData.y
                        size: modelData.s
                        name: modelData.i
                        rotation: modelData.r
                        color: Theme.accent
                        opacity: 0.55
                    }
                }

                Column {
                    anchors { left: parent.left; leftMargin: 34; verticalCenter: parent.verticalCenter }
                    spacing: 2
                    Row {
                        spacing: 10
                        Text {
                            id: bannerTitle
                            text: "Nokturn"
                            color: Qt.lighter(Theme.accent, 1.12)
                            font.family: Theme.fontDisplay
                            font.pixelSize: 64
                            font.weight: Font.Black
                            layer.enabled: true
                            layer.effect: MultiEffect {
                                shadowEnabled: true
                                shadowColor: Theme.accent
                                shadowBlur: 0.8
                                shadowOpacity: 0.9
                                shadowHorizontalOffset: 0
                                shadowVerticalOffset: 0
                            }
                        }
                        Icon {
                            anchors.verticalCenter: bannerTitle.verticalCenter
                            anchors.verticalCenterOffset: -10
                            name: "heartOutline"
                            size: 30
                            rotation: 14
                            color: Theme.accent
                        }
                    }
                    Text {
                        leftPadding: 4
                        text: qsTr("Your music · Your rules")
                        color: Theme.text
                        font.pixelSize: 17
                        font.weight: Font.Medium
                    }
                }
            }
        }

        Reveal {
            ColumnLayout {
                width: parent.width
                spacing: 4
                Text {
                    text: Theme.greeting()
                    color: Theme.text
                    font.family: Theme.fontDisplay
                    font.pixelSize: 38
                    font.weight: Font.Bold
                }
                Text {
                    text: Theme.cat ? qsTr("Back for more music? ♡")
                                       : qsTr("Your music from disk, YouTube Music and radio — in one place.")
                    color: Theme.textDim
                    font.pixelSize: 15
                }
            }
        }

        Reveal {
            delay: 70
            GridLayout {
                id: topRow
                width: parent.width
                columns: width > 800 ? 2 : 1
                columnSpacing: 18
                rowSpacing: 14
                HeroCard { Layout.alignment: Qt.AlignTop }
                // The built-in lists, beside the card (below it, in two columns, in a narrow window)
                Grid {
                    id: shortcutList
                    Layout.preferredWidth: topRow.columns > 1 ? 230 : topRow.width
                    Layout.alignment: Qt.AlignTop
                    columns: topRow.columns > 1 ? 1 : 2
                    columnSpacing: 14
                    readonly property real cellW: (Layout.preferredWidth - columnSpacing * (columns - 1)) / columns
                    Repeater {
                        model: page.shortcuts
                        PlaylistRow {
                            required property var modelData
                            width: shortcutList.cellW
                            playlist: modelData.p
                            onClicked: page.openPick(modelData.p, modelData.index)
                            onPlayClicked: page.playPick(modelData.p, modelData.index)
                        }
                    }
                }
            }
        }

        Reveal {
            delay: 140
            ColumnLayout {
                width: parent.width
                spacing: 18
                SectionHeader {
                    Layout.fillWidth: true
                    title: qsTr("Your albums")
                    action: qsTr("Show all")
                    onActionClicked: Nav.navigate("library", { tab: 1 })
                }
                Grid {
                    id: albumGrid
                    Layout.fillWidth: true
                    Layout.preferredWidth: 100
                    columns: Math.max(3, Math.floor((width + columnSpacing) / 200))
                    columnSpacing: 22
                    readonly property real cellW: (width - columnSpacing * (columns - 1)) / columns

                    Repeater {
                        model: Library.recentAlbums.slice(0, albumGrid.columns)
                        AlbumCard {
                            required property var modelData
                            required property int index
                            width: albumGrid.cellW
                            shadow: albumShadow
                            title: modelData.title
                            subtitle: modelData.artist + (modelData.year > 0 ? " · " + modelData.year : "")
                            colorA: modelData.colorA
                            colorB: modelData.colorB
                            seed: modelData.seed
                            cover: modelData.cover
                            playing: Player.playing && Player.albumKey === modelData.key
                            revealDelay: 320 + index * 60
                            onClicked: Nav.navigate("playlist", { kind: "album", key: modelData.key })
                            onPlayClicked: {
                                if (playing) { Player.toggle(); return }
                                const rows = Library.tracksOfAlbum(modelData.key)
                                Player.playFrom(rows, rows[0])
                            }
                        }
                    }
                }
            }
        }

        Reveal {
            delay: 210
            visible: recentRepeater.count > 0
            ColumnLayout {
                width: parent.width
                spacing: 6
                SectionHeader {
                    Layout.fillWidth: true
                    Layout.bottomMargin: 10
                    title: qsTr("Recently added")
                    action: qsTr("Library")
                    onActionClicked: Nav.navigate("library")
                }
                Repeater {
                    id: recentRepeater
                    model: Library.tracks.revision >= 0 ? Library.recentlyAdded(5) : []
                    TrackRow {
                        required property var modelData
                        required property int index
                        Layout.fillWidth: true
                        trackIndex: modelData
                        queue: Library.recentlyAdded(50)
                        number: index + 1
                        revealDelay: 400 + index * 50
                    }
                }
            }
        }

        Reveal {
            delay: 280
            visible: page.recommended.length > 0 || Recommendations.loading
            ColumnLayout {
                width: parent.width
                spacing: 14

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Recommended for you")
                            color: Theme.text
                            font.family: Theme.fontDisplay
                            font.pixelSize: 22
                            font.weight: Font.DemiBold
                        }
                        Text {
                            Layout.fillWidth: true
                            text: Recommendations.basedOn.length > 0
                                  ? qsTr("Because you listen to %1").arg(Recommendations.basedOn.slice(0, 3).join(", "))
                                  : qsTr("Looking for music that fits your taste…")
                            color: Theme.textDim
                            font.pixelSize: 13
                            elide: Text.ElideRight
                        }
                    }
                    Spinner {
                        visible: Recommendations.loading
                        size: 18
                        color: Theme.textDim
                    }
                    IconButton {
                        icon: "refresh"
                        iconSize: 20
                        implicitWidth: 38; implicitHeight: 38
                        showDot: false
                        enabled: !Recommendations.loading
                        opacity: enabled ? 1 : 0.4
                        onClicked: Recommendations.refresh()
                    }
                    PillButton {
                        kind: "ghost"
                        icon: "play"
                        text: qsTr("Play")
                        fontSize: 13
                        implicitHeight: 38
                        enabled: page.recommended.length > 0
                        onClicked: Player.playFrom(page.recommended, page.recommended[0], "recs")
                    }
                }

                Grid {
                    id: recGrid
                    Layout.fillWidth: true
                    Layout.preferredWidth: 100
                    columns: width > 1100 ? 3 : width > 620 ? 2 : 1
                    columnSpacing: 10
                    readonly property real cellW: (width - columnSpacing * (columns - 1)) / columns
                    readonly property int folded: columns * 4

                    Repeater {
                        model: page.recommended.slice(0, page.allRecommended ? page.recommended.length : recGrid.folded)
                        TrackRow {
                            required property var modelData
                            required property int index
                            width: recGrid.cellW
                            compact: true
                            trackIndex: modelData
                            number: index + 1
                            queue: page.recommended
                            context: "recs"
                            revealDelay: 220 + Math.min(index, 12) * 35
                        }
                    }
                }

                PillButton {
                    visible: page.recommended.length > recGrid.folded
                    kind: "ghost"
                    icon: page.allRecommended ? "" : "expandMore"
                    text: page.allRecommended ? qsTr("Show less") : qsTr("Show more (%1)").arg(page.recommended.length)
                    fontSize: 13
                    onClicked: page.allRecommended = !page.allRecommended
                }
            }
        }

        Reveal {
            delay: 350
            visible: page.fullPlaylists.length > 0
            ColumnLayout {
                width: parent.width
                spacing: 18
                SectionHeader {
                    Layout.fillWidth: true
                    title: qsTr("Playlists")
                    subtitle: qsTr("Yours, from YouTube Music and Spotify")
                }
                Grid {
                    id: playlistGrid
                    Layout.fillWidth: true
                    Layout.preferredWidth: 100
                    columns: Math.max(3, Math.floor((width + columnSpacing) / 200))
                    columnSpacing: 22
                    rowSpacing: 18
                    readonly property real cellW: (width - columnSpacing * (columns - 1)) / columns

                    Repeater {
                        model: page.fullPlaylists
                        AlbumCard {
                            required property var modelData
                            required property int index
                            width: playlistGrid.cellW
                            shadow: albumShadow
                            title: modelData.p.name
                            subtitle: modelData.p.count !== undefined && modelData.p.count >= 0
                                      ? qsTr("%n tracks", "", modelData.p.count) : ""
                            colorA: modelData.p.colorA
                            colorB: modelData.p.colorB
                            seed: modelData.p.seed
                            cover: modelData.p.cover || ""
                            mosaic: modelData.p.mosaic || []
                            revealDelay: 380 + Math.min(index, 8) * 50
                            onClicked: page.openPick(modelData.p, modelData.index)
                            onPlayClicked: page.playPick(modelData.p, modelData.index)
                        }
                    }
                }
            }
        }

        Reveal {
            delay: 420
            ColumnLayout {
                width: parent.width
                spacing: 16
                SectionHeader {
                    Layout.fillWidth: true
                    title: qsTr("Your sources")
                    subtitle: qsTr("Where Nokturn gets its music")
                    action: qsTr("Manage")
                    onActionClicked: Nav.navigate("sources")
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14
                    Repeater {
                        model: Library.sources
                        SourceMini {}
                    }
                }
            }
        }
    }
}
