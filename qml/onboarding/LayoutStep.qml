import QtQuick
import QtQuick.Layouts
import Nokturn

// Step 2: window layout — the sidebar and the "Now playing" panel, on live thumbnails.
Item {
    id: root

    ColumnLayout {
        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
        spacing: 26

        StepTitle {
            title: qsTr("How should the window be laid out?")
            subtitle: qsTr("Click the variant you like — the app changes right away. The thumbnails show how it looks in action.")
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 28

            ColumnLayout {
                Layout.fillWidth: true
                Layout.preferredWidth: 100
                spacing: 12
                GroupLabel { text: qsTr("Sidebar") }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14
                    OptionCard {
                        title: qsTr("Full")
                        description: qsTr("Icons with names and your playlists always at hand.")
                        selected: !Nav.sidebarCompact
                        onClicked: Nav.sidebarCompact = false
                        LayoutPreview {
                            anchors.fill: parent
                            compact: false
                            showQueue: Nav.queueOpen
                            dynamicAccent: Theme.dynamicAccent
                            ambient: Theme.ambient
                        }
                    }
                    OptionCard {
                        title: qsTr("Compact")
                        description: qsTr("Icons only — more room for covers and lists.")
                        selected: Nav.sidebarCompact
                        onClicked: Nav.sidebarCompact = true
                        LayoutPreview {
                            anchors.fill: parent
                            compact: true
                            showQueue: Nav.queueOpen
                            dynamicAccent: Theme.dynamicAccent
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
                GroupLabel { text: qsTr("“Now playing” panel") }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14
                    OptionCard {
                        title: qsTr("Visible")
                        description: qsTr("Cover, queue and lyrics on the right.")
                        selected: Nav.queueOpen
                        onClicked: Nav.queueOpen = true
                        LayoutPreview {
                            anchors.fill: parent
                            compact: Nav.sidebarCompact
                            showQueue: true
                            dynamicAccent: Theme.dynamicAccent
                            ambient: Theme.ambient
                        }
                    }
                    OptionCard {
                        title: qsTr("Hidden")
                        description: qsTr("You open it with a button on the player bar.")
                        selected: !Nav.queueOpen
                        onClicked: Nav.queueOpen = false
                        LayoutPreview {
                            anchors.fill: parent
                            compact: Nav.sidebarCompact
                            showQueue: false
                            dynamicAccent: Theme.dynamicAccent
                            ambient: Theme.ambient
                        }
                    }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("Tip: press F11 for full-screen mode with a big cover and lyrics.")
            color: Theme.textFaint
            font.pixelSize: 13
        }
    }
}
