import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import QtQuick.Controls.Basic
import Nokturn

// Widok playlisty, albumu lub wykonawcy. Lista jest zwirtualizowana (ListView), bo playlisty
// z YouTube Music potrafią mieć tysiące utworów.
Item {
    id: page
    property var args: ({})

    readonly property var info: {
        Library.tracks.revision
        const a = args || {}
        if (a.kind === "album") {
            const al = Library.albums.find(x => x.key === a.key) || { title: "", artist: "", year: 0 }
            return { label: "ALBUM", title: al.title, meta: [al.artist].concat(al.year > 0 ? [String(al.year)] : []),
                     colorA: al.colorA || Theme.accent, colorB: al.colorB || Theme.accent2, seed: al.seed || 0,
                     cover: al.cover || "", icon: "", round: false,
                     rows: Library.tracksOfAlbum(a.key) }
        }
        if (a.kind === "artist") {
            const ar = Library.artists.find(x => x.name === a.artist) || {}
            return { label: "WYKONAWCA", title: a.artist, meta: [],
                     colorA: ar.colorA || Theme.accent, colorB: ar.colorB || Theme.accent2, seed: ar.seed || 0,
                     cover: ar.cover || "", icon: "", round: true,
                     rows: Library.tracksOfArtist(a.artist) }
        }
        const index = a.index || 0
        const p = Library.playlists[index]
        if (!p)
            return { label: "PLAYLISTA", title: "", meta: [], colorA: Theme.accent, colorB: Theme.accent2,
                     seed: 0, cover: "", icon: "music", round: false, rows: [], loading: false }
        const yt = p.kind === "ytplaylist"
        const meta = yt ? "YouTube Music"
                   : p.kind === "youtube" ? (YouTube.connected ? "Polubione na Twoim koncie YouTube" : "Przykładowe utwory z YouTube Music")
                   : "Twoja playlista"
        return { label: yt ? "PLAYLISTA · YOUTUBE MUSIC" : "PLAYLISTA", title: p.name, meta: [meta],
                 colorA: p.colorA, colorB: p.colorB, seed: p.seed, cover: p.cover || "", icon: p.icon, round: false,
                 rows: Library.playlistRows(index), loading: Library.playlistLoading(index) }
    }
    readonly property int totalSeconds: Library.totalDuration(info.rows)
    readonly property real pad: 34

    readonly property bool playingHere: Player.playing && info.rows.indexOf(Player.currentIndex) >= 0

    // Kolorowa poświata za nagłówkiem (z lekką paralaksą)
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
        model: page.info.rows

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
                                          Theme.plural(page.info.rows.length, "utwór", "utwory", "utworów"),
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
                            onClicked: page.playingHere ? Player.toggle() : Player.playFrom(page.info.rows, page.info.rows[0])
                        }
                        IconButton {
                            icon: "shuffle"
                            iconSize: 24
                            implicitWidth: 46; implicitHeight: 46
                            active: Player.shuffle
                            onClicked: {
                                Player.shuffle = true
                                const rows = page.info.rows
                                if (rows.length) Player.playFrom(rows, rows[Math.floor(Math.random() * rows.length)])
                            }
                        }
                        IconButton {
                            icon: "more"
                            iconSize: 24
                            implicitWidth: 46; implicitHeight: 46
                            showDot: false
                            onClicked: Nav.toast("Menu kontekstowe pojawi się wkrótce", "more")
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
                            text: page.info.loading ? "Wczytywanie playlisty…" : "Nic tu jeszcze nie ma"
                            color: Theme.text
                            font.pixelSize: 18
                            font.weight: Font.DemiBold
                        }
                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: page.info.loading ? "Pobieramy utwory z YouTube Music" : "Polub utwór sercem, a pojawi się w tym miejscu."
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
            queue: page.info.rows
            revealDelay: index < 16 ? 180 + index * 30 : 0
        }
    }
}