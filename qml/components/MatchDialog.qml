import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Nokturn

// Spotify track played from the wrong YouTube Music version: a few search results ("artist title",
// editable) to pick the right one from. The choice is remembered and plays straight away.
Popup {
    id: root
    property string trackId: ""
    property var track: ({})
    property var results: []
    property bool loading: false
    property bool manual: false       // the current match was chosen by the user
    property string error: ""

    // t: the track's map (TrackModel.get)
    function show(t) {
        track = t
        trackId = t.id
        manual = Spotify.manualMatch(t.id)
        query.text = Spotify.matchQuery(t.id)
        results = []
        error = ""
        open()
        search()
    }

    function search() {
        loading = true
        error = ""
        Spotify.findMatches(trackId, query.text)
    }

    function choose(videoId) {
        Spotify.chooseMatch(trackId, videoId)
        Nav.toast(qsTr("Saved — this version will play from now on"), "check")
        close()
    }

    anchors.centerIn: Overlay.overlay
    width: Math.min(620, (Overlay.overlay ? Overlay.overlay.width : 700) - 60)
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

    Connections {
        target: Spotify
        function onMatchesFound(trackId, results, error) {
            if (trackId !== root.trackId)
                return
            root.loading = false
            root.results = results
            root.error = error
        }
    }

    contentItem: ColumnLayout {
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            Icon { name: "search"; size: 22; color: Theme.accent }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text {
                    Layout.fillWidth: true
                    text: qsTr("Choose the YouTube Music version")
                    color: Theme.text
                    font.family: Theme.fontDisplay
                    font.pixelSize: 20
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }
                Text {
                    Layout.fillWidth: true
                    text: [root.track.title, root.track.artist, root.track.durationText]
                          .filter(s => !!s).join("  ·  ")
                    color: Theme.textDim
                    font.pixelSize: 13
                    elide: Text.ElideRight
                }
            }
            IconButton { icon: "close"; showDot: false; onClicked: root.close() }
        }

        InputField {
            id: query
            Layout.fillWidth: true
            icon: "search"
            placeholderText: qsTr("Artist and title")
            onAccepted: root.search()
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(400, Math.max(120, list.implicitHeight))

            Flickable {
                id: flick
                anchors.fill: parent
                clip: true
                contentHeight: list.implicitHeight
                boundsBehavior: Flickable.StopAtBounds
                visible: !root.loading && root.results.length > 0
                ScrollBar.vertical: ThinScrollBar {}

                Column {
                    id: list
                    width: flick.width
                    spacing: 4

                    Repeater {
                        model: root.results
                        delegate: Item {
                            id: entry
                            required property var modelData
                            readonly property bool far: Math.abs(modelData.durationDiff) > 12
                            width: list.width
                            height: 58

                            Rectangle {
                                anchors.fill: parent
                                radius: 12
                                color: entry.modelData.current ? Theme.alpha(Theme.accent, 0.11) : Theme.alpha(Theme.text, 0.055)
                                opacity: entry.modelData.current || pick.containsMouse ? 1 : 0
                                Behavior on opacity { NumberAnimation { duration: Theme.fast } }
                            }
                            MouseArea {
                                id: pick
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.choose(entry.modelData.videoId)
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 12
                                spacing: 12

                                ArtCover {
                                    Layout.preferredWidth: 42
                                    Layout.preferredHeight: 42
                                    radius: 8
                                    source: entry.modelData.cover || ""
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    Text {
                                        Layout.fillWidth: true
                                        text: entry.modelData.title
                                        color: entry.modelData.current ? Theme.accent : Theme.text
                                        font.pixelSize: 14
                                        font.weight: Font.Medium
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: [entry.modelData.artist, entry.modelData.album].filter(s => !!s).join("  ·  ")
                                        color: Theme.textDim
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                    }
                                }
                                Text {
                                    visible: entry.modelData.current
                                    text: qsTr("In use")
                                    color: Theme.accent
                                    font.pixelSize: 11
                                    font.weight: Font.DemiBold
                                }
                                Rectangle {
                                    implicitWidth: kindLabel.implicitWidth + 16
                                    implicitHeight: 22
                                    radius: height / 2
                                    color: Theme.alpha(Theme.text, 0.07)
                                    Text {
                                        id: kindLabel
                                        anchors.centerIn: parent
                                        text: entry.modelData.kind === "song" ? qsTr("Song") : qsTr("Video")
                                        color: Theme.textDim
                                        font.pixelSize: 11
                                        font.weight: Font.DemiBold
                                    }
                                }
                                Text {
                                    Layout.preferredWidth: 44
                                    horizontalAlignment: Text.AlignRight
                                    text: entry.modelData.durationText
                                    color: entry.far ? Theme.warning : Theme.textDim
                                    font.pixelSize: 13
                                    font.features: { "tnum": 1 }
                                }
                            }
                        }
                    }
                }
            }

            Spinner {
                anchors.centerIn: parent
                visible: root.loading
                size: 26
                color: Theme.textDim
            }

            Text {
                anchors.centerIn: parent
                width: parent.width - 40
                visible: !root.loading && root.results.length === 0
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: root.error !== "" ? root.error : qsTr("Nothing found — try a different search")
                color: Theme.textDim
                font.pixelSize: 14
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Text {
                Layout.fillWidth: true
                text: root.manual ? qsTr("You chose this track's version yourself.")
                                                        : qsTr("A duration in yellow differs from Spotify's by more than 12 s.")
                color: Theme.textFaint
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
            PillButton {
                kind: "ghost"
                text: qsTr("Match again")
                icon: "refresh"
                onClicked: {
                    Spotify.rematch(root.trackId)
                    root.close()
                }
            }
        }
    }
}
