import QtQuick
import QtQuick.Layouts
import Nokturn

// Step 3: accent color (from the cover / fixed) and background (live / calm).
Item {
    id: root

    ColumnLayout {
        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
        spacing: 26

        StepTitle {
            title: qsTr("Colors and mood")
            subtitle: qsTr("Nokturn can take on the colors of the playing track's cover and gently pulse with light to the music.")
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 28

            ColumnLayout {
                Layout.fillWidth: true
                Layout.preferredWidth: 100
                spacing: 12
                GroupLabel { text: qsTr("Accent color") }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14
                    OptionCard {
                        title: qsTr("From the cover")
                        description: qsTr("Buttons, sliders and light smoothly change color along with the music.")
                        selected: Theme.dynamicAccent
                        onClicked: Theme.dynamicAccent = true
                        LayoutPreview {
                            anchors.fill: parent
                            compact: Nav.sidebarCompact
                            showQueue: Nav.queueOpen
                            dynamicAccent: true
                            ambient: Theme.ambient
                        }
                    }
                    OptionCard {
                        title: qsTr("Fixed")
                        description: qsTr("One calm sage color — regardless of the track.")
                        selected: !Theme.dynamicAccent
                        onClicked: Theme.dynamicAccent = false
                        LayoutPreview {
                            anchors.fill: parent
                            compact: Nav.sidebarCompact
                            showQueue: Nav.queueOpen
                            dynamicAccent: false
                            ambient: Theme.ambient
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillHeight: true
                Layout.topMargin: 28
                width: 1
                color: Qt.rgba(1, 1, 1, 0.07)
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.preferredWidth: 100
                spacing: 12
                GroupLabel { text: qsTr("Background") }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14
                    OptionCard {
                        title: qsTr("Live")
                        description: qsTr("A soft glow behind the interface, pulsing with the music.")
                        selected: Theme.ambient
                        onClicked: Theme.ambient = true
                        LayoutPreview {
                            anchors.fill: parent
                            compact: Nav.sidebarCompact
                            showQueue: Nav.queueOpen
                            dynamicAccent: Theme.dynamicAccent
                            ambient: true
                        }
                    }
                    OptionCard {
                        title: qsTr("Calm")
                        description: qsTr("A plain dark background — less motion, less work for the graphics card.")
                        selected: !Theme.ambient
                        onClicked: Theme.ambient = false
                        LayoutPreview {
                            anchors.fill: parent
                            compact: Nav.sidebarCompact
                            showQueue: Nav.queueOpen
                            dynamicAccent: Theme.dynamicAccent
                            ambient: false
                        }
                    }
                }
            }
        }
    }
}
