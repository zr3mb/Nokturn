import QtQuick
import QtQuick.Controls.Basic
import Nokturn

// Sidebar: logo, menu with a "floating" selection indicator, playlists.
Panel {
    id: root
    readonly property bool compact: Nav.sidebarCompact
    property Item selectedItem: null
    tint: Theme.alpha(Theme.surface, 0.55)

    function isPlaylist(i) {
        return Nav.page === "playlist" && Nav.args && Nav.args.kind === "playlist" && Nav.args.index === i
    }

    Item {
        id: logo
        width: parent.width
        height: 78

        LogoMark {
            id: mark
            x: 21
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            anchors { left: mark.right; leftMargin: 12; verticalCenter: parent.verticalCenter }
            text: "Nokturn"
            color: Theme.text
            font.family: Theme.fontDisplay
            font.pixelSize: 22
            font.weight: Font.DemiBold
            font.letterSpacing: 0.2
            opacity: root.compact ? 0 : 1
            Behavior on opacity { NumberAnimation { duration: 220 } }
        }
    }

    Flickable {
        id: flick
        anchors { top: logo.bottom; left: parent.left; right: parent.right; bottom: footer.top }
        contentWidth: width
        contentHeight: listRoot.height + 16
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ThinScrollBar {}

        Item {
            id: listRoot
            x: 12
            width: flick.width - 24
            height: col.height

            // Selection indicator — springs between items
            Rectangle {
                id: indicator
                readonly property Item target: root.selectedItem
                visible: target !== null && target.selected
                width: parent.width
                y: target ? target.y : 0
                height: target ? target.height : 44
                radius: 12
                color: Theme.alpha(Theme.text, 0.085)
                Behavior on y { SpringAnimation { spring: 3.4; damping: 0.3; epsilon: 0.2 } }
                Behavior on height { NumberAnimation { duration: 260; easing.type: Easing.OutCubic } }

                Rectangle {
                    anchors { left: parent.left; verticalCenter: parent.verticalCenter }
                    width: 3
                    height: parent.height * 0.46
                    radius: 2
                    color: Theme.accent
                }
            }

            Column {
                id: col
                width: parent.width
                spacing: 2

                Text {
                    width: parent.width
                    height: 28
                    leftPadding: 14
                    verticalAlignment: Text.AlignVCenter
                    text: root.compact ? "" : qsTr("MENU")
                    color: Theme.textFaint
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    font.letterSpacing: 1.6
                }

                Repeater {
                    model: [
                        { key: "home", label: qsTr("Home"), icon: "home" },
                        { key: "search", label: qsTr("Search"), icon: "search" },
                        { key: "library", label: qsTr("Library"), icon: "library" },
                        { key: "sources", label: qsTr("Sources"), icon: "sources" },
                        { key: "equalizer", label: qsTr("Equalizer"), icon: "equalizer" }
                    ]

                    SidebarItem {
                        id: navItem
                        required property var modelData
                        icon: modelData.icon
                        label: modelData.label
                        compact: root.compact
                        selected: Nav.page === modelData.key
                        onSelectedChanged: if (selected) root.selectedItem = navItem
                        Component.onCompleted: if (selected) root.selectedItem = navItem
                        onClicked: Nav.navigate(modelData.key)
                    }
                }

                Item { width: 1; height: 14 }

                Item {
                    width: parent.width
                    height: 30

                    Text {
                        anchors { left: parent.left; leftMargin: 14; verticalCenter: parent.verticalCenter }
                        text: qsTr("PLAYLISTS")
                        color: Theme.textFaint
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                        font.letterSpacing: 1.6
                        opacity: root.compact ? 0 : 1
                        Behavior on opacity { NumberAnimation { duration: 200 } }
                    }
                    Rectangle {
                        anchors.centerIn: parent
                        width: 24; height: 1
                        color: Qt.rgba(1, 1, 1, 0.1)
                        opacity: root.compact ? 1 : 0
                        Behavior on opacity { NumberAnimation { duration: 200 } }
                    }
                    IconButton {
                        anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                        width: 28; height: 28
                        icon: "add"
                        iconSize: 18
                        showDot: false
                        visible: !root.compact
                        onClicked: Nav.toast(qsTr("Creating playlists is coming soon"), "add")
                    }
                }

                Repeater {
                    model: Library.playlists

                    SidebarItem {
                        id: plItem
                        required property var modelData
                        required property int index
                        cover: true
                        compact: root.compact
                        label: modelData.name
                        subtitle: {
                            Library.tracks.revision
                            if (modelData.kind === "ytplaylist")
                                return "YouTube Music" + (modelData.count >= 0 ? " · " + qsTr("%n tracks", "", modelData.count) : "")
                            if (modelData.kind === "spplaylist" || modelData.kind === "spotifyliked")
                                return "Spotify · " + qsTr("%n tracks", "", Library.playlistRows(index).length)
                            const n = Library.playlistRows(index).length
                            return qsTr("Playlist · %n tracks", "", n)
                        }
                        colorA: modelData.colorA
                        colorB: modelData.colorB
                        seed: modelData.seed
                        coverIcon: modelData.icon
                        coverSource: modelData.cover || ""
                        coverMosaic: modelData.mosaic || []
                        selected: root.isPlaylist(index)
                        onSelectedChanged: if (selected) root.selectedItem = plItem
                        Component.onCompleted: if (selected) root.selectedItem = plItem
                        onClicked: Nav.navigate("playlist", { kind: "playlist", index: index })
                    }
                }
            }
        }
    }

    Item {
        id: footer
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: footerCol.height + 24

        Rectangle {
            anchors { top: parent.top; left: parent.left; right: parent.right; leftMargin: 18; rightMargin: 18 }
            height: 1
            color: Qt.rgba(1, 1, 1, 0.06)
        }

        Column {
            id: footerCol
            x: 12
            y: 12
            width: parent.width - 24
            spacing: 2

            SidebarItem {
                icon: "tune"
                label: qsTr("Settings")
                compact: root.compact
                ownHighlight: true
                selected: Nav.page === "settings"
                onClicked: Nav.navigate("settings")
            }
        }
    }
}
