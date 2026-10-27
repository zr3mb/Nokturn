import QtQuick
import QtQuick.Controls.Basic
import Nokturn

// Sidebar: logo, menu with a "floating" selection indicator, playlists.
Panel {
    id: root
    readonly property bool compact: Nav.sidebarCompact
    property Item selectedItem: null
    tint: Theme.alpha(Theme.surface, 0.55)
    roundedClip: false   // the logo, the list and the footer all stay clear of the corners

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
            id: brand
            anchors { left: mark.right; leftMargin: 12; verticalCenter: parent.verticalCenter }
            text: "Nokturn"
            color: Theme.cat ? Theme.accent : Theme.text
            font.family: Theme.fontDisplay
            font.pixelSize: 22
            font.weight: Theme.cat ? Font.Bold : Font.DemiBold
            font.letterSpacing: 0.2
            opacity: root.compact ? 0 : 1
            Behavior on opacity { NumberAnimation { duration: 220 } }
        }
        Icon {
            visible: Theme.cat
            anchors { left: brand.right; leftMargin: 8; verticalCenter: brand.verticalCenter; verticalCenterOffset: -6 }
            name: "heart"
            size: 14
            rotation: 12
            color: Theme.accent
            opacity: brand.opacity
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
                        { key: "room", label: qsTr("Listening room"), icon: "group" },
                        { key: "equalizer", label: qsTr("Equalizer"), icon: "equalizer" },
                        { key: "stats", label: qsTr("Stats"), icon: "chart" }
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
                        id: newPlaylist
                        anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                        width: 28; height: 28
                        icon: "add"
                        iconSize: 18
                        showDot: false
                        visible: !root.compact
                        onClicked: Nav.createPlaylist([])
                    }
                    IconButton {
                        anchors { right: newPlaylist.left; verticalCenter: parent.verticalCenter }
                        width: 28; height: 28
                        icon: "folder"
                        iconSize: 16
                        showDot: false
                        visible: !root.compact
                        onClicked: Nav.createFolder(-1)
                    }
                }

                // The playlists in their groups: the built-in lists, the user's folders, one group per source
                // (Library.playlistGroups); a group's header folds it away.
                Repeater {
                    model: {
                        const out = []
                        Library.playlistGroups.forEach(g => {
                            if (g.key !== "builtin" && !root.compact)
                                out.push({ header: true, group: g })
                            if (!g.collapsed || root.compact || g.key === "builtin")
                                g.items.forEach(i => out.push({ header: false, index: i }))
                        })
                        return out
                    }

                    Item {
                        id: row
                        required property var modelData
                        readonly property bool selected: plItem.selected
                        width: col.width
                        height: modelData.header ? 30 : plItem.height

                        // a group's header
                        Item {
                            anchors.fill: parent
                            visible: row.modelData.header === true
                            readonly property var group: row.modelData.group || ({})
                            HoverHandler { id: headerHover }
                            TapHandler { onTapped: Library.setGroupCollapsed(parent.group.key, !parent.group.collapsed) }
                            Icon {
                                id: chevron
                                anchors { left: parent.left; leftMargin: 12; verticalCenter: parent.verticalCenter }
                                name: parent.group.folderId ? "folder" : "chevronRight"
                                size: parent.group.folderId ? 13 : 14
                                rotation: !parent.group.folderId && !parent.group.collapsed ? 90 : 0
                                color: Theme.textFaint
                                Behavior on rotation { NumberAnimation { duration: 160 } }
                            }
                            Text {
                                anchors { left: chevron.right; leftMargin: 7; right: folderTools.left; rightMargin: 4; verticalCenter: parent.verticalCenter }
                                text: (parent.group.name || "").toUpperCase() + (parent.group.collapsed ? "  ·  " + (parent.group.items || []).length : "")
                                color: Theme.textFaint
                                font.pixelSize: 11
                                font.weight: Font.DemiBold
                                font.letterSpacing: 1.3
                                elide: Text.ElideRight
                            }
                            Row {
                                id: folderTools
                                anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                                opacity: headerHover.hovered ? 1 : 0
                                Behavior on opacity { NumberAnimation { duration: 140 } }
                                IconButton {
                                    visible: !!parent.parent.group.folderId
                                    width: 24; height: 24
                                    icon: "edit"
                                    iconSize: 14
                                    showDot: false
                                    onClicked: Nav.editFolder(parent.parent.group.folderId)
                                }
                                IconButton {
                                    visible: !!parent.parent.group.folderId
                                    width: 24; height: 24
                                    icon: parent.parent.group.collapsed ? "chevronRight" : "expandMore"
                                    iconSize: 14
                                    showDot: false
                                    onClicked: Library.setGroupCollapsed(parent.parent.group.key, !parent.parent.group.collapsed)
                                }
                            }
                        }

                        SidebarItem {
                            id: plItem
                            readonly property int plIndex: row.modelData.index ?? -1
                            readonly property var pl: Library.playlists[plIndex] || ({})
                            visible: !row.modelData.header
                            width: parent.width
                            opacity: pl.hidden ? 0.55 : 1
                            cover: true
                            compact: root.compact
                            label: pl.name || ""
                            subtitle: {
                                if (Library.tracks.revision < 0 || plIndex < 0)   // never / a header: re-run when the tracks change
                                    return ""
                                if (pl.kind === "ytplaylist")
                                    return "YouTube Music" + (pl.count >= 0 ? " · " + qsTr("%n tracks", "", pl.count) : "")
                                if (pl.kind === "spplaylist" || pl.kind === "spotifyliked")
                                    return "Spotify · " + qsTr("%n tracks", "", Library.playlistRows(plIndex).length)
                                if (pl.kind === "scplaylist")
                                    return "SoundCloud" + (pl.count >= 0 ? " · " + qsTr("%n tracks", "", pl.count) : "")
                                if (pl.kind === "scliked")
                                    return "SoundCloud · " + qsTr("%n tracks", "", Library.playlistRows(plIndex).length)
                                if (pl.kind === "shared")
                                    return qsTr("Shared · %n tracks", "", pl.count)
                                return qsTr("Playlist · %n tracks", "", Library.playlistRows(plIndex).length)
                            }
                            colorA: pl.colorA || "#888888"
                            colorB: pl.colorB || "#444444"
                            seed: pl.seed || 0
                            coverIcon: pl.icon || ""
                            coverSource: pl.cover || ""
                            coverMosaic: pl.mosaic || []
                            selected: !row.modelData.header && root.isPlaylist(plIndex)
                            onSelectedChanged: if (selected) root.selectedItem = row
                            Component.onCompleted: if (selected) root.selectedItem = row
                            onClicked: Nav.navigate("playlist", { kind: "playlist", index: plIndex })
                        }
                    }
                }

                // the hidden playlists, on request
                Text {
                    visible: Library.hiddenCount > 0 && !root.compact
                    width: parent.width
                    height: 30
                    leftPadding: 14
                    verticalAlignment: Text.AlignVCenter
                    text: Library.showHidden ? qsTr("Hide hidden playlists") : qsTr("Show hidden playlists (%1)").arg(Library.hiddenCount)
                    color: hiddenHover.hovered ? Theme.textDim : Theme.textFaint
                    font.pixelSize: 12
                    HoverHandler { id: hiddenHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: Library.showHidden = !Library.showHidden }
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
