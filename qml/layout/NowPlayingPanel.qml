import QtQuick
import QtQuick.Layouts
import QtQuick.Shapes
import QtQuick.Effects
import QtQuick.Controls.Basic
import Nokturn

// Right panel: cover with a vinyl record sliding out, track details, queue.
Panel {
    id: root
    tint: Theme.alpha(Theme.surface, 0.55)

    readonly property var upNext: Player.upNext
    readonly property bool lyricsTab: Nav.panelTab === "lyrics"

    Connections {
        target: Player
        function onCurrentChanged() { enter.restart() }
    }

    ParallelAnimation {
        id: enter
        NumberAnimation { target: details; property: "opacity"; from: 0; to: 1; duration: 450; easing.type: Easing.OutCubic }
        NumberAnimation { target: detailsShift; property: "y"; from: 14; to: 0; duration: 650; easing.type: Easing.OutQuint }
        NumberAnimation { target: art; property: "scale"; from: 0.94; to: 1; duration: 700; easing.type: Easing.OutBack }
    }

    Item {
        id: inner
        width: 344
        height: parent.height
        opacity: root.width > 240 ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 220 } }

        Flickable {
            id: flick
            anchors.fill: parent
            contentWidth: width
            contentHeight: column.implicitHeight + 44
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ThinScrollBar {}

            ColumnLayout {
                id: column
                x: 22
                y: 20
                width: inner.width - 44
                spacing: 16

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Now playing")
                        color: Theme.text
                        font.family: Theme.fontDisplay
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                    }
                    IconButton {
                        icon: "close"
                        iconSize: 18
                        showDot: false
                        onClicked: Nav.queueOpen = false
                    }
                }

                // Cover + vinyl record
                Item {
                    id: art
                    Layout.fillWidth: true
                    Layout.preferredHeight: coverSize
                    readonly property real coverSize: width * 0.8

                    Item {
                        id: vinyl
                        width: art.coverSize * 0.94
                        height: width
                        anchors.verticalCenter: parent.verticalCenter
                        x: Player.playing ? art.width - width : (art.coverSize - width) / 2
                        Behavior on x { NumberAnimation { duration: 1100; easing.type: Easing.OutQuint } }

                        Item {
                            id: disc
                            anchors.fill: parent

                            NumberAnimation on rotation {
                                from: 0; to: 360
                                duration: 5200
                                loops: Animation.Infinite
                                running: true
                                paused: !Player.playing
                            }

                            Shape {
                                anchors.fill: parent
                                preferredRendererType: Shape.CurveRenderer
                                readonly property real r: width / 2

                                ShapePath {
                                    strokeColor: "transparent"
                                    fillGradient: RadialGradient {
                                        centerX: vinyl.width / 2; centerY: vinyl.height / 2
                                        focalX: vinyl.width / 2; focalY: vinyl.height / 2
                                        centerRadius: vinyl.width / 2; focalRadius: 0
                                        GradientStop { position: 0; color: "#1d1e22" }
                                        GradientStop { position: 1; color: "#0b0b0d" }
                                    }
                                    PathAngleArc { centerX: vinyl.width / 2; centerY: vinyl.height / 2; radiusX: vinyl.width / 2; radiusY: vinyl.width / 2; startAngle: 0; sweepAngle: 360 }
                                }
                                ShapePath {
                                    strokeColor: Qt.rgba(1, 1, 1, 0.05)
                                    strokeWidth: 1
                                    fillColor: "transparent"
                                    PathAngleArc { centerX: vinyl.width / 2; centerY: vinyl.height / 2; radiusX: vinyl.width * 0.46; radiusY: vinyl.width * 0.46; startAngle: 0; sweepAngle: 360 }
                                    PathAngleArc { centerX: vinyl.width / 2; centerY: vinyl.height / 2; radiusX: vinyl.width * 0.41; radiusY: vinyl.width * 0.41; startAngle: 0; sweepAngle: 360 }
                                    PathAngleArc { centerX: vinyl.width / 2; centerY: vinyl.height / 2; radiusX: vinyl.width * 0.36; radiusY: vinyl.width * 0.36; startAngle: 0; sweepAngle: 360 }
                                    PathAngleArc { centerX: vinyl.width / 2; centerY: vinyl.height / 2; radiusX: vinyl.width * 0.31; radiusY: vinyl.width * 0.31; startAngle: 0; sweepAngle: 360 }
                                    PathAngleArc { centerX: vinyl.width / 2; centerY: vinyl.height / 2; radiusX: vinyl.width * 0.26; radiusY: vinyl.width * 0.26; startAngle: 0; sweepAngle: 360 }
                                }
                                ShapePath {
                                    strokeColor: "transparent"
                                    fillGradient: LinearGradient {
                                        x1: 0; y1: 0; x2: vinyl.width; y2: vinyl.height
                                        GradientStop { position: 0; color: Theme.accent }
                                        GradientStop { position: 1; color: Theme.accent2 }
                                    }
                                    PathAngleArc { centerX: vinyl.width / 2; centerY: vinyl.height / 2; radiusX: vinyl.width * 0.17; radiusY: vinyl.width * 0.17; startAngle: 0; sweepAngle: 360 }
                                }
                                ShapePath {
                                    strokeColor: "transparent"
                                    fillColor: Qt.rgba(1, 1, 1, 0.5)
                                    PathAngleArc { centerX: vinyl.width * 0.5 + vinyl.width * 0.08; centerY: vinyl.height / 2; radiusX: vinyl.width * 0.012; radiusY: vinyl.width * 0.012; startAngle: 0; sweepAngle: 360 }
                                }
                                ShapePath {
                                    strokeColor: "transparent"
                                    fillColor: "#0b0b0d"
                                    PathAngleArc { centerX: vinyl.width / 2; centerY: vinyl.height / 2; radiusX: vinyl.width * 0.02; radiusY: vinyl.width * 0.02; startAngle: 0; sweepAngle: 360 }
                                }
                            }
                        }

                        // Static glare
                        Shape {
                            anchors.fill: parent
                            preferredRendererType: Shape.CurveRenderer
                            ShapePath {
                                strokeColor: "transparent"
                                fillGradient: LinearGradient {
                                    x1: vinyl.width * 0.2; y1: 0; x2: vinyl.width * 0.8; y2: vinyl.height
                                    GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, 0) }
                                    GradientStop { position: 0.45; color: Qt.rgba(1, 1, 1, 0.07) }
                                    GradientStop { position: 0.55; color: Qt.rgba(1, 1, 1, 0) }
                                }
                                PathAngleArc { centerX: vinyl.width / 2; centerY: vinyl.height / 2; radiusX: vinyl.width / 2; radiusY: vinyl.width / 2; startAngle: 0; sweepAngle: 360 }
                            }
                        }
                    }

                    ArtCover {
                        id: bigCover
                        width: art.coverSize
                        height: art.coverSize
                        radius: 16
                        colorA: Player.colorA
                        colorB: Player.colorB
                        seed: Player.seed
                        source: Player.cover
                        scale: 1 + Player.level * 0.018
                        layer.enabled: inner.opacity > 0   // the shadow's textures are released while the panel is closed
                        layer.effect: MultiEffect {
                            shadowEnabled: true
                            shadowBlur: 1.0
                            shadowOpacity: 0.6
                            shadowHorizontalOffset: 6
                            shadowVerticalOffset: 10
                        }
                    }
                }

                ColumnLayout {
                    id: details
                    Layout.fillWidth: true
                    spacing: 6
                    transform: Translate { id: detailsShift }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3
                            Text {
                                Layout.fillWidth: true
                                text: Player.title
                                color: Theme.text
                                font.family: Theme.fontDisplay
                                font.pixelSize: 22
                                font.weight: Font.Bold
                                elide: Text.ElideRight
                            }
                            ArtistLink {
                                Layout.fillWidth: true
                                text: Player.artist
                                artistId: Player.artistId
                                linkEnabled: Player.hasTrack
                                font.pixelSize: 14
                                elide: Text.ElideRight
                            }
                        }
                        IconButton {
                            icon: Player.liked ? "heart" : "heartOutline"
                            active: Player.liked
                            enabled: Player.hasTrack
                            showDot: false
                            onClicked: {
                                Player.toggleLiked()
                                Nav.toast(Player.liked ? qsTr("Added to Liked") : qsTr("Removed from Liked"), "heart")
                            }
                        }
                    }

                    Flow {
                        Layout.fillWidth: true
                        Layout.topMargin: 4
                        spacing: 8
                        SourceBadge { source: Player.source; visible: Player.hasTrack }
                        Rectangle {
                            visible: Player.quality !== ""
                            width: qualityText.implicitWidth + 18
                            height: 24
                            radius: 12
                            color: Theme.alpha(Theme.text, 0.06)
                            Text {
                                id: qualityText
                                anchors.centerIn: parent
                                text: Player.quality
                                color: Theme.textDim
                                font.pixelSize: 11
                                font.weight: Font.DemiBold
                            }
                        }
                    }
                }

                Visualizer {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 100
                    Layout.preferredHeight: 44
                    bars: 40
                    spacing: 2
                    barWidth: (width - spacing * (bars - 1)) / bars
                    symmetric: true
                    centered: true
                    colorTop: Theme.alpha(Theme.accent, 0.9)
                    colorBottom: Theme.alpha(Theme.accent, 0.35)
                    minHeight: 2
                    opacity: Player.playing ? 1 : 0.35
                    Behavior on opacity { NumberAnimation { duration: 600 } }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: Qt.rgba(1, 1, 1, 0.06)
                }

                RowLayout {
                    Layout.fillWidth: true
                    SegmentedTabs {
                        model: [qsTr("Queue"), qsTr("Lyrics", "panel tab")]
                        currentIndex: Nav.panelTab === "lyrics" ? 1 : 0
                        onActivated: i => Nav.panelTab = i === 1 ? "lyrics" : "queue"
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        visible: !root.lyricsTab
                        text: Player.shuffle ? qsTr("Shuffle") : qsTr("In order")
                        color: Theme.textFaint
                        font.pixelSize: 12
                    }
                    Spinner {
                        visible: root.lyricsTab && (Lyrics.state === "loading" || Lyrics.aligning)
                        size: 14
                        color: Theme.textDim
                    }
                }

                LyricsView {
                    visible: root.lyricsTab
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.max(300, flick.height - 120)
                    fontSize: 17
                    lineSpacing: 8
                }

                Text {
                    visible: !root.lyricsTab && root.upNext.length === 0
                    Layout.fillWidth: true
                    text: Player.hasTrack ? qsTr("That's the end of the queue") : qsTr("The queue is empty — pick something from the library")
                    color: Theme.textFaint
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                }

                Repeater {
                    model: root.lyricsTab ? [] : root.upNext

                    Item {
                        id: q
                        required property var modelData
                        required property int index
                        readonly property var t: Library.tracks.revision >= 0 ? Library.tracks.get(modelData) : ({})
                        Layout.fillWidth: true
                        implicitHeight: 52

                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: -2
                            radius: 10
                            color: Theme.alpha(Theme.text, 0.06)
                            opacity: qHover.hovered ? 1 : 0
                            Behavior on opacity { NumberAnimation { duration: Theme.fast } }
                        }
                        HoverHandler { id: qHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: Player.jumpTo(q.modelData) }

                        ArtCover {
                            id: qCover
                            width: 42; height: 42
                            anchors.verticalCenter: parent.verticalCenter
                            radius: 8
                            colorA: q.t.colorA
                            colorB: q.t.colorB
                            seed: q.t.seed
                            source: q.t.cover || ""
                            Rectangle {
                                anchors.fill: parent
                                radius: 8
                                color: "#80000000"
                                opacity: qHover.hovered ? 1 : 0
                                Behavior on opacity { NumberAnimation { duration: Theme.fast } }
                                Icon { anchors.centerIn: parent; name: "play"; size: 18; color: "#ffffff" }
                            }
                        }
                        Column {
                            anchors {
                                left: qCover.right; leftMargin: 12
                                right: qDur.left; rightMargin: 8
                                verticalCenter: parent.verticalCenter
                            }
                            spacing: 2
                            Text {
                                width: parent.width
                                text: q.t.title
                                color: Theme.text
                                font.pixelSize: 14
                                font.weight: Font.Medium
                                elide: Text.ElideRight
                            }
                            Text {
                                width: parent.width
                                text: q.t.artist
                                color: Theme.textDim
                                font.pixelSize: 12
                                elide: Text.ElideRight
                            }
                        }
                        Text {
                            id: qDur
                            anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                            text: q.t.durationText
                            color: q.t.duration > 0 ? Theme.textFaint : Theme.warning
                            font.pixelSize: q.t.duration > 0 ? 12 : 10
                            font.features: { "tnum": 1 }
                        }
                    }
                }
            }
        }
    }
}
