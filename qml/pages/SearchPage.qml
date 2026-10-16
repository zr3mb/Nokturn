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
        Library.tracks.revision
        return Library.search(query, filterKeys[filter])
    }
    // Online search in the YouTube Music catalog for "All" and "YouTube Music"
    readonly property bool onlineEnabled: query.trim().length > 1 && (filter === 0 || filter === 2)
    readonly property var online: {
        Library.tracks.revision
        return onlineEnabled ? Library.onlineResults() : []
    }

    onQueryChanged: onlineTimer.restart()
    onFilterChanged: onlineTimer.restart()

    Timer {
        id: onlineTimer
        interval: 450
        onTriggered: if (page.onlineEnabled) YouTube.search(page.query)
    }

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

    component MoodTile: Item {
        id: tile
        required property var modelData
        required property int index
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
        TapHandler { onTapped: Nav.toast(qsTr("Moods will be available once sources are connected"), "sparkle") }

        ArtCover {
            anchors.fill: parent
            radius: 18
            colorA: tile.modelData.a
            colorB: tile.modelData.b
            seed: tile.modelData.seed
            scale: tileHover.hovered ? 1.03 : 1
            Behavior on scale { NumberAnimation { duration: 380; easing.type: Easing.OutCubic } }
        }
        Text {
            anchors { left: parent.left; bottom: parent.bottom; margins: 18 }
            text: tile.modelData.name
            color: "#f6f2ec"
            font.family: Theme.fontDisplay
            font.pixelSize: 21
            font.weight: Font.Bold
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
                model: page.results
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

            RowLayout {
                visible: page.onlineEnabled && (page.online.length > 0 || YouTube.searching)
                Layout.fillWidth: true
                Layout.topMargin: page.results.length > 0 ? 26 : 0
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
                visible: page.results.length === 0 && page.online.length === 0 && !YouTube.searching
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
                title: qsTr("Browse moods")
                subtitle: qsTr("Or just start typing")
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
                    model: page.moods
                    MoodTile { width: moodGrid.cellW }
                }
            }
        }
    }
}
