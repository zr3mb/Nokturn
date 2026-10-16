import QtQuick
import QtQuick.Layouts
import Nokturn

// Custom title bar: back/forward navigation, search, profile, window buttons.
Item {
    id: root
    readonly property var win: Window.window

    // Window dragging and double-click to maximize
    Item {
        anchors.fill: parent
        DragHandler {
            target: null
            onActiveChanged: if (active) root.win.startSystemMove()
        }
        TapHandler {
            onDoubleTapped: root.win.visibility === Window.Maximized ? root.win.showNormal() : root.win.showMaximized()
        }
    }

    Row {
        anchors { left: parent.left; leftMargin: 18; verticalCenter: parent.verticalCenter }
        spacing: 4

        IconButton {
            icon: Nav.sidebarCompact ? "menu" : "menuOpen"
            iconSize: 20
            showDot: false
            onClicked: Nav.sidebarCompact = !Nav.sidebarCompact
        }
        Item { width: 8; height: 1 }
        IconButton {
            icon: "chevronLeft"
            iconSize: 22
            enabled: Nav.canGoBack
            showDot: false
            onClicked: Nav.back()
        }
        IconButton {
            icon: "chevronRight"
            iconSize: 22
            enabled: Nav.canGoForward
            showDot: false
            onClicked: Nav.forward()
        }
    }

    // Search pill
    Rectangle {
        id: search
        anchors.centerIn: parent
        width: Math.min(480, root.width * 0.34)
        height: 38
        radius: 19
        color: Theme.alpha(Theme.text, searchMouse.containsMouse ? 0.085 : 0.055)
        border.width: 1
        border.color: Theme.alpha(Theme.text, searchMouse.containsMouse ? 0.12 : 0.06)
        scale: searchMouse.pressed ? 0.985 : 1
        Behavior on color { ColorAnimation { duration: Theme.fast } }
        Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutBack } }

        Icon {
            id: searchIcon
            x: 14
            anchors.verticalCenter: parent.verticalCenter
            name: "search"
            size: 19
            color: searchMouse.containsMouse ? Theme.text : Theme.textDim
        }
        Text {
            anchors { left: searchIcon.right; leftMargin: 10; verticalCenter: parent.verticalCenter }
            text: qsTr("Search tracks, artists, albums…")
            color: Theme.textFaint
            font.pixelSize: 14
        }
        Rectangle {
            anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
            width: kbd.implicitWidth + 12
            height: 22
            radius: 6
            color: Theme.alpha(Theme.text, 0.06)
            border.width: 1
            border.color: Theme.alpha(Theme.text, 0.08)
            Text {
                id: kbd
                anchors.centerIn: parent
                text: "Ctrl K"
                color: Theme.textFaint
                font.pixelSize: 11
                font.weight: Font.DemiBold
            }
        }
        MouseArea {
            id: searchMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: Nav.openSearch()
        }
    }

    Row {
        anchors { right: parent.right; rightMargin: 10; verticalCenter: parent.verticalCenter }
        spacing: 10

        // Profile
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: profileRow.implicitWidth + 10
            height: 36
            radius: 18
            color: Theme.alpha(Theme.text, profileMouse.containsMouse ? 0.09 : 0.05)
            Behavior on color { ColorAnimation { duration: Theme.fast } }

            Row {
                id: profileRow
                anchors { left: parent.left; leftMargin: 4; verticalCenter: parent.verticalCenter }
                spacing: 8
                Rectangle {
                    width: 28; height: 28; radius: 14
                    gradient: Gradient {
                        GradientStop { position: 0; color: Theme.accent }
                        GradientStop { position: 1; color: Theme.accent2 }
                    }
                    Icon {
                        anchors.centerIn: parent
                        name: "person"
                        size: 17
                        color: "#f4efe8"
                    }
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Profile")
                    color: Theme.text
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    rightPadding: 6
                }
            }
            MouseArea {
                id: profileMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: Nav.navigate("settings")
            }
        }

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: 1; height: 20
            color: Qt.rgba(1, 1, 1, 0.08)
        }

        WindowControls {
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    Shortcut { sequence: "Ctrl+K"; onActivated: Nav.openSearch() }
}
