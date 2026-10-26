import QtQuick
import Nokturn

// Phone layout: the bottom navigation bar in place of the sidebar.
Rectangle {
    id: root
    color: Theme.alpha(Theme.bg, 0.97)
    signal moreRequested()

    Rectangle {
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 1
        color: Theme.alpha(Theme.text, 0.07)
    }

    Row {
        anchors.fill: parent
        Repeater {
            model: [
                { key: "home", label: qsTr("Home"), icon: "home" },
                { key: "search", label: qsTr("Search"), icon: "search" },
                { key: "library", label: qsTr("Library"), icon: "library" },
                { key: "room", label: qsTr("Room"), icon: "group" },
                { key: "more", label: qsTr("More"), icon: "menu" }
            ]

            Item {
                id: tab
                required property var modelData
                readonly property bool selected: modelData.key === "more"
                                                 ? ["sources", "equalizer", "stats", "settings"].indexOf(Nav.page) >= 0
                                                 : Nav.page === modelData.key
                width: root.width / 5
                height: root.height

                Column {
                    anchors.centerIn: parent
                    spacing: 3
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 52; height: 28
                        radius: 14
                        color: tab.selected ? Theme.alpha(Theme.accent, 0.18) : "transparent"
                        Behavior on color { ColorAnimation { duration: Theme.fast } }
                        Icon {
                            anchors.centerIn: parent
                            name: tab.modelData.icon
                            size: 21
                            color: tab.selected ? Theme.accent : Theme.textDim
                        }
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: tab.modelData.label
                        color: tab.selected ? Theme.text : Theme.textDim
                        font.pixelSize: 11
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        if (tab.modelData.key === "more")
                            root.moreRequested()
                        else if (tab.modelData.key === "search")
                            Nav.openSearch()
                        else
                            Nav.navigate(tab.modelData.key)
                    }
                }
            }
        }
    }
}
