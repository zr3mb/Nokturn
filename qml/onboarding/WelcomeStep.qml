import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import Nokturn

// Step 1: welcome.
Item {
    id: root

    property real t: 0
    NumberAnimation on t { from: 0; to: Math.PI * 2; duration: 3200; loops: Animation.Infinite; running: root.visible }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width, 980)
        spacing: 22

        // Big, "breathing" logo with a glow
        Item {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 140
            Layout.preferredHeight: 140

            Rectangle {
                anchors.centerIn: parent
                width: 150 + 14 * Math.sin(root.t); height: width
                radius: width / 2
                color: Theme.alpha(Theme.accent, 0.18)
                layer.enabled: true
                layer.effect: MultiEffect { blurEnabled: true; blur: 1.0; blurMax: 48 }
            }
            Rectangle {
                id: tile
                anchors.centerIn: parent
                width: 112; height: 112
                radius: 36
                gradient: Gradient {
                    GradientStop { position: 0; color: Qt.lighter(Theme.accent, 1.08) }
                    GradientStop { position: 1; color: Qt.darker(Theme.accent2, 1.05) }
                }
                Row {
                    anchors.centerIn: parent
                    spacing: 9
                    Repeater {
                        model: 4
                        Rectangle {
                            required property int index
                            anchors.verticalCenter: parent.verticalCenter
                            width: 9
                            radius: 4.5
                            height: 18 + 34 * Math.abs(Math.sin(root.t + index * 0.9))
                            color: "#f4f0ea"
                        }
                    }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Welcome to Nokturn")
            color: Theme.text
            font.family: Theme.fontDisplay
            font.pixelSize: 54
            font.weight: Font.Bold
        }
        Text {
            Layout.fillWidth: true
            Layout.maximumWidth: 640
            Layout.alignment: Qt.AlignHCenter
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Files from your disk and YouTube Music in one calm player. Let's set it up for you — it takes a minute, and you can change everything later in Settings.")
            color: Theme.textDim
            font.pixelSize: 16
            wrapMode: Text.WordWrap
            lineHeight: 1.15
        }
        // The language can be picked right away (also in Settings → Appearance).
        SegmentedTabs {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 6
            model: ["English", "Polski"]
            currentIndex: Language.effective === "pl" ? 1 : 0
            onActivated: i => Language.language = ["en", "pl"][i]
        }

        GridLayout {
            Layout.fillWidth: true
            Layout.topMargin: 18
            columns: 4
            columnSpacing: 14
            rowSpacing: 14

            Repeater {
                model: [
                    { icon: "folder", title: qsTr("Your library"), text: qsTr("Music folders, covers and tags — scanned in the background.") },
                    { icon: "playCircle", title: "YouTube Music", text: qsTr("Liked songs, playlists and search from your account.") },
                    { icon: "mix", title: qsTr("Mixes and lyrics"), text: qsTr("DJ-style smooth transitions and live lyrics.") },
                    { icon: "equalizer", title: qsTr("Equalizer"), text: qsTr("7 bands, presets and a limiter — sound your way.") }
                ]
                Rectangle {
                    id: tileCard
                    required property var modelData
                    required property int index
                    Layout.fillWidth: true
                    Layout.preferredHeight: 150
                    radius: 18
                    color: Theme.alpha(Theme.surface, 0.7)
                    border.width: 1
                    border.color: Qt.rgba(1, 1, 1, 0.06)

                    // delayed entrance
                    opacity: 0
                    transform: Translate { id: shift; y: 24 }
                    Component.onCompleted: enter.start()
                    SequentialAnimation {
                        id: enter
                        PauseAnimation { duration: 260 + tileCard.index * 90 }
                        ParallelAnimation {
                            NumberAnimation { target: tileCard; property: "opacity"; to: 1; duration: 500; easing.type: Easing.OutCubic }
                            NumberAnimation { target: shift; property: "y"; to: 0; duration: 700; easing.type: Easing.OutQuint }
                        }
                    }

                    ColumnLayout {
                        anchors { fill: parent; margins: 18 }
                        spacing: 8
                        Rectangle {
                            width: 40; height: 40; radius: 12
                            color: Theme.alpha(Theme.accent, 0.16)
                            Icon { anchors.centerIn: parent; name: tileCard.modelData.icon; size: 21; color: Theme.accent }
                        }
                        Text {
                            Layout.fillWidth: true
                            text: tileCard.modelData.title
                            color: Theme.text
                            font.pixelSize: 15
                            font.weight: Font.DemiBold
                        }
                        Text {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            text: tileCard.modelData.text
                            color: Theme.textDim
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }
        }
    }
}
