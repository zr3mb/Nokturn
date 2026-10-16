import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import Nokturn

// Biblioteka. Listy są zwirtualizowane (ListView / GridView), więc tworzą tylko
// widoczne wiersze — działa płynnie również przy tysiącach utworów.
Item {
    id: page
    property var args: ({})
    property int tab: args && args.tab !== undefined ? args.tab : 0
    readonly property real pad: 34
    // Pliki lokalne + polubione w YouTube Music
    readonly property var libraryRows: {
        Library.tracks.revision
        return Library.libraryRows()
    }

    component LibraryHeader: ColumnLayout {
        spacing: 22

        RowLayout {
            Layout.fillWidth: true
            spacing: 16

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                Text {
                    Layout.fillWidth: true
                    text: "Biblioteka"
                    color: Theme.text
                    font.family: Theme.fontDisplay
                    font.pixelSize: 38
                    font.weight: Font.Bold
                }
                Text {
                    Layout.fillWidth: true
                    text: Library.scanning
                          ? Library.scanStatus
                          : Theme.plural(Library.localCount, "utwór", "utwory", "utworów") + "  ·  "
                            + Theme.plural(Library.albums.filter(a => a.source === "local").length, "album", "albumy", "albumów") + "  ·  "
                            + Theme.plural(Library.artists.filter(a => a.local).length, "wykonawca", "wykonawców", "wykonawców")
                            + (YouTube.connected ? "  ·  " + Theme.plural(YouTube.likedCount, "polubiony", "polubione", "polubionych") + " w YouTube Music" : "")
                    color: Library.scanning ? Theme.accent : Theme.textDim
                    font.pixelSize: 14
                }
            }

            PillButton {
                kind: "secondary"
                icon: "add"
                text: "Dodaj folder"
                onClicked: Nav.addFolder()
            }
        }

        Item {
            Layout.fillWidth: true
            implicitHeight: tabs.implicitHeight

            Rectangle {
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                height: 1
                color: Qt.rgba(1, 1, 1, 0.06)
            }
            SegmentedTabs {
                id: tabs
                style: "underline"
                model: ["Utwory", "Albumy", "Wykonawcy"]
                currentIndex: page.tab
                onActivated: i => page.tab = i
            }
        }

        Rectangle {
            visible: Library.localCount === 0 && !Library.scanning && !YouTube.connected
            Layout.fillWidth: true
            implicitHeight: 72
            radius: 16
            color: Theme.alpha(Theme.accent, 0.08)
            border.width: 1
            border.color: Theme.alpha(Theme.accent, 0.18)

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 14
                spacing: 14
                Icon { name: "folder"; size: 24; color: Theme.accent }
                Text {
                    Layout.fillWidth: true
                    text: "Nie masz jeszcze lokalnych plików. Dodaj folder z muzyką — Nokturn odczyta tagi i okładki."
                    color: Theme.textDim
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                }
                PillButton {
                    icon: "folder"
                    text: "Wybierz folder"
                    onClicked: Nav.addFolder()
                }
            }
        }
    }

    // ---- Utwory ---------------------------------------------------------------
    ListView {
        id: tracksView
        anchors.fill: parent
        visible: page.tab === 0
        clip: true
        leftMargin: page.pad
        rightMargin: page.pad
        bottomMargin: page.pad
        cacheBuffer: 800
        boundsBehavior: Flickable.StopAtBounds
        flickDeceleration: 3200
        maximumFlickVelocity: 4200
        ScrollBar.vertical: ThinScrollBar {}
        model: page.libraryRows

        header: Item {
            width: tracksView.width - page.pad * 2
            height: page.pad + hdr.implicitHeight + 12 + listHead.implicitHeight
            LibraryHeader {
                id: hdr
                y: page.pad
                width: parent.width
            }
            TrackListHeader {
                id: listHead
                anchors.bottom: parent.bottom
                width: parent.width
            }
        }

        delegate: TrackRow {
            required property var modelData
            required property int index
            width: tracksView.width - page.pad * 2
            trackIndex: modelData
            number: index + 1
            queue: page.libraryRows
            revealDelay: index < 16 ? index * 28 : 0
        }
    }

    // ---- Albumy i wykonawcy -------------------------------------------------------
    component CardGrid: GridView {
        id: grid
        property real minCell: 200
        readonly property real inner: width - page.pad * 2
        readonly property int columns: Math.max(3, Math.floor(inner / minCell))

        anchors.fill: parent
        clip: true
        leftMargin: page.pad
        rightMargin: page.pad
        bottomMargin: page.pad
        cellWidth: inner / columns
        cellHeight: cellWidth - 22 + 62 + 30
        cacheBuffer: 600
        boundsBehavior: Flickable.StopAtBounds
        flickDeceleration: 3200
        maximumFlickVelocity: 4200
        ScrollBar.vertical: ThinScrollBar {}

        header: Item {
            width: grid.inner
            height: page.pad + gridHdr.implicitHeight + 28
            LibraryHeader {
                id: gridHdr
                y: page.pad
                width: parent.width
            }
        }
    }

    CardGrid {
        id: albumsView
        visible: page.tab === 1
        model: Library.albums

        delegate: Item {
            id: albumCell
            required property var modelData
            required property int index
            width: albumsView.cellWidth
            height: albumsView.cellHeight

            AlbumCard {
                width: albumsView.cellWidth - 22
                title: albumCell.modelData.title
                subtitle: albumCell.modelData.artist + (albumCell.modelData.year > 0 ? " · " + albumCell.modelData.year : "")
                colorA: albumCell.modelData.colorA
                colorB: albumCell.modelData.colorB
                seed: albumCell.modelData.seed
                cover: albumCell.modelData.cover
                playing: Player.playing && Player.albumKey === albumCell.modelData.key
                revealDelay: albumCell.index < 12 ? albumCell.index * 45 : 0
                onClicked: Nav.navigate("playlist", { kind: "album", key: albumCell.modelData.key })
                onPlayClicked: {
                    if (playing) { Player.toggle(); return }
                    const rows = Library.tracksOfAlbum(albumCell.modelData.key)
                    Player.playFrom(rows, rows[0])
                }
            }
        }
    }

    CardGrid {
        id: artistsView
        visible: page.tab === 2
        minCell: 190
        model: Library.artists

        delegate: Item {
            id: artistCell
            required property var modelData
            required property int index
            width: artistsView.cellWidth
            height: artistsView.cellHeight

            AlbumCard {
                width: artistsView.cellWidth - 22
                round: true
                title: artistCell.modelData.name
                subtitle: "Wykonawca · " + Theme.plural(artistCell.modelData.count, "utwór", "utwory", "utworów")
                colorA: artistCell.modelData.colorA
                colorB: artistCell.modelData.colorB
                seed: artistCell.modelData.seed
                cover: artistCell.modelData.cover
                playing: Player.playing && Player.artist === artistCell.modelData.name
                revealDelay: artistCell.index < 12 ? artistCell.index * 45 : 0
                onClicked: Nav.navigate("playlist", { kind: "artist", artist: artistCell.modelData.name })
                onPlayClicked: {
                    if (playing) { Player.toggle(); return }
                    const rows = Library.tracksOfArtist(artistCell.modelData.name)
                    Player.playFrom(rows, rows[0])
                }
            }
        }
    }
}
