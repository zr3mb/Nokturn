import QtQuick
import QtQuick.Layouts
import Nokturn

// Your listening in numbers (ListeningHistory): this month, this year or all time — minutes, plays,
// the artists, tracks and genres you played most, and when in the day you listen.
Item {
    id: page
    property var args: ({})
    property int period: 0
    readonly property var periods: ["month", "year", "all"]
    readonly property var stats: {
        // Never true: the read makes the page follow new plays (a bare read is compiled away).
        if (ListeningHistory.revision < 0 || Library.genresRevision < 0)
            return ({})
        return ListeningHistory.stats(periods[period])
    }
    readonly property bool empty: (stats.plays || 0) === 0

    component Tile: Rectangle {
        property string value: ""
        property string label: ""
        Layout.fillWidth: true
        Layout.preferredHeight: 104
        radius: 18
        color: Theme.alpha(Theme.text, 0.04)
        border.width: 1
        border.color: Theme.alpha(Theme.text, 0.06)
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 2
            Text {
                text: parent.parent.value
                color: Theme.text
                font.family: Theme.fontDisplay
                font.pixelSize: 34
                font.weight: Font.Bold
                font.features: { "tnum": 1 }
            }
            Text {
                text: parent.parent.label
                color: Theme.textDim
                font.pixelSize: 13
            }
        }
    }

    component Heading: Text {
        Layout.topMargin: 6
        color: Theme.text
        font.family: Theme.fontDisplay
        font.pixelSize: 20
        font.weight: Font.DemiBold
    }

    function hours(minutes) {
        return minutes >= 120 ? qsTr("%1 h").arg(Math.round(minutes / 60)) : qsTr("%1 min").arg(minutes)
    }

    PageFlick {
        anchors.fill: parent
        spacing: 26

        Reveal {
            GridLayout {
                width: parent.width
                columns: Nav.mobile ? 1 : 2   // phone: the periods below the title
                columnSpacing: 18
                rowSpacing: 12
                Text {
                    Layout.fillWidth: true
                    text: qsTr("Your stats")
                    color: Theme.text
                    font.family: Theme.fontDisplay
                    font.pixelSize: 38
                    font.weight: Font.Bold
                }
                SegmentedTabs {
                    model: [qsTr("This month"), qsTr("This year"), qsTr("All time")]
                    currentIndex: page.period
                    onActivated: i => page.period = i
                }
            }
        }

        // ---- nothing yet ----
        Reveal {
            delay: 60
            visible: page.empty
            Text {
                width: parent.width
                text: qsTr("Nothing here yet — a track counts after 30 seconds of listening.")
                color: Theme.textDim
                font.pixelSize: 15
                wrapMode: Text.WordWrap
            }
        }

        // ---- totals ----
        Reveal {
            delay: 60
            visible: !page.empty
            GridLayout {
                width: parent.width
                columns: width > 760 ? 4 : 2
                columnSpacing: 14
                rowSpacing: 14
                Tile { value: page.hours(page.stats.minutes || 0); label: qsTr("of music") }
                Tile { value: String(page.stats.plays || 0); label: qsTr("plays") }
                Tile { value: String(page.stats.artists || 0); label: qsTr("artists") }
                Tile { value: String(page.stats.days || 0); label: qsTr("days with music") }
            }
        }

        // ---- top artists ----
        Reveal {
            delay: 100
            visible: (page.stats.topArtists || []).length > 0
            ColumnLayout {
                width: parent.width
                spacing: 14
                Heading { text: qsTr("Top artists") }
                Flow {
                    Layout.fillWidth: true
                    spacing: 22
                    Repeater {
                        model: page.stats.topArtists || []
                        Item {
                            id: artistCard
                            required property var modelData
                            required property int index
                            width: 140
                            height: 196
                            ArtCover {
                                width: 140
                                height: 140
                                radius: 70
                                seed: artistCard.index
                                colorA: Theme.accent
                                colorB: Theme.accent2
                                source: artistCard.modelData.cover || ""
                            }
                            Text {
                                y: 150
                                width: parent.width
                                text: (artistCard.index + 1) + ". " + artistCard.modelData.name
                                color: Theme.text
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                                horizontalAlignment: Text.AlignHCenter
                            }
                            Text {
                                y: 172
                                width: parent.width
                                text: qsTr("%n plays", "", artistCard.modelData.plays) + " · " + page.hours(artistCard.modelData.minutes)
                                color: Theme.textDim
                                font.pixelSize: 12
                                elide: Text.ElideRight
                                horizontalAlignment: Text.AlignHCenter
                            }
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: Nav.openArtist(artistCard.modelData.name, artistCard.modelData.artistId)
                            }
                        }
                    }
                }
            }
        }

        // ---- top tracks ----
        Reveal {
            delay: 140
            visible: (page.stats.topTracks || []).length > 0
            ColumnLayout {
                width: parent.width
                spacing: 6
                Heading { text: qsTr("Top tracks") }
                Repeater {
                    model: page.stats.topTracks || []
                    RowLayout {
                        id: trackRow
                        required property var modelData
                        required property int index
                        readonly property bool playable: modelData.row >= 0
                        Layout.fillWidth: true
                        spacing: 14
                        TrackRow {
                            visible: trackRow.playable
                            Layout.fillWidth: true
                            number: trackRow.index + 1
                            trackIndex: trackRow.playable ? trackRow.modelData.row : 0
                            compact: true
                            queue: (page.stats.topTracks || []).filter(t => t.row >= 0).map(t => t.row)
                            context: "stats"
                        }
                        // Not in the model any more (an old search result): shown, not playable.
                        Column {
                            visible: !trackRow.playable
                            Layout.fillWidth: true
                            Text { text: trackRow.modelData.title; color: Theme.text; font.pixelSize: 14 }
                            Text { text: trackRow.modelData.artist; color: Theme.textDim; font.pixelSize: 12 }
                        }
                        Text {
                            text: qsTr("%n plays", "", trackRow.modelData.plays)
                            color: Theme.textDim
                            font.pixelSize: 13
                        }
                    }
                }
            }
        }

        // ---- genres and the time of day ----
        Reveal {
            delay: 180
            visible: !page.empty
            GridLayout {
                width: parent.width
                columns: width > 820 ? 2 : 1
                columnSpacing: 40
                rowSpacing: 26

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    spacing: 12
                    visible: (page.stats.topGenres || []).length > 0
                    Heading { text: qsTr("Genres") }
                    Repeater {
                        model: page.stats.topGenres || []
                        ColumnLayout {
                            id: genre
                            required property var modelData
                            Layout.fillWidth: true
                            spacing: 5
                            RowLayout {
                                Layout.fillWidth: true
                                Text { Layout.fillWidth: true; text: genre.modelData.name; color: Theme.text; font.pixelSize: 14 }
                                Text { text: Math.round(genre.modelData.share * 100) + "%"; color: Theme.textDim; font.pixelSize: 13 }
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                height: 6
                                radius: 3
                                color: Theme.alpha(Theme.text, 0.08)
                                Rectangle {
                                    width: parent.width * genre.modelData.share
                                    height: parent.height
                                    radius: 3
                                    color: Theme.accent
                                }
                            }
                        }
                    }
                }

                ColumnLayout {
                    id: day
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    spacing: 12
                    readonly property var byHour: page.stats.byHour || []
                    readonly property real peak: Math.max(1, ...byHour)
                    Heading { text: qsTr("When you listen") }
                    Row {
                        Layout.fillWidth: true
                        height: 120
                        spacing: 3
                        Repeater {
                            model: day.byHour
                            Item {
                                required property var modelData
                                required property int index
                                width: (day.width - 23 * 3) / 24
                                height: 120
                                Rectangle {
                                    anchors.bottom: parent.bottom
                                    width: parent.width
                                    height: Math.max(3, 120 * modelData / day.peak)
                                    radius: 3
                                    color: modelData === day.peak ? Theme.accent : Theme.alpha(Theme.accent, 0.45)
                                }
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Repeater {
                            model: ["0:00", "6:00", "12:00", "18:00", "24:00"]
                            Text {
                                required property string modelData
                                Layout.fillWidth: true
                                text: modelData
                                color: Theme.textFaint
                                font.pixelSize: 11
                            }
                        }
                    }
                }
            }
        }
    }
}
