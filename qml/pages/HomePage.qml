import QtQuick
import QtQuick.Layouts
import Nokturn

Item {
    id: page
    property var args: ({})

    readonly property var quickPicks: Library.playlists.concat([
        { name: qsTr("All tracks"), kind: "all", icon: "library", colorA: "#a3b3c1", colorB: "#3b4a57", seed: 0 },
        { name: qsTr("Random mix"), kind: "mix", icon: "shuffle", colorA: "#b7aac9", colorB: "#463c5c", seed: 0 },
        { name: qsTr("Night Jazz — live"), kind: "radio", icon: "live", colorA: "#b9a889", colorB: "#4a4232", seed: 0 }
    ])

    // Picked from YouTube Music radios of the tracks that say the most about the taste (Recommendations).
    readonly property var recommended: {
        Library.tracks.revision
        YouTube.resultsRevision   // the picks are appended to the model, not rebuilt into it
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
        if (p.kind === "mix") Player.shuffle = true
        const start = p.kind === "mix" ? rows[Math.floor(Math.random() * rows.length)] : rows[0]
        Player.playFrom(rows, start)
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
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 7; height: 7; radius: 3.5
                    color: mini.status === "connected" ? Theme.success
                         : mini.status === "disconnected" ? Theme.danger : Theme.textFaint
                }
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
                    text: qsTr("Your music from disk, YouTube Music and radio — in one place.")
                    color: Theme.textDim
                    font.pixelSize: 15
                }
            }
        }

        Reveal {
            delay: 70
            HeroCard { width: parent.width }
        }

        Reveal {
            delay: 140
            Grid {
                id: quick
                width: parent.width
                columns: width > 1000 ? 3 : 2
                columnSpacing: 14
                rowSpacing: 14
                readonly property real cellW: (width - columnSpacing * (columns - 1)) / columns

                Repeater {
                    model: page.quickPicks
                    QuickTile {
                        required property var modelData
                        required property int index
                        width: quick.cellW
                        title: modelData.name
                        colorA: modelData.colorA
                        colorB: modelData.colorB
                        seed: modelData.seed
                        coverIcon: modelData.icon
                        coverSource: modelData.cover || ""
                        coverMosaic: modelData.mosaic || []
                        revealDelay: 180 + index * 55
                        onClicked: page.openPick(modelData, index)
                        onPlayClicked: page.playPick(modelData, index)
                    }
                }
            }
        }

        Reveal {
            delay: 175
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
            delay: 210
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

        Reveal {
            delay: 280
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
            delay: 350
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
                    model: { Library.tracks.revision; return Library.recentlyAdded(5) }
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
    }
}
