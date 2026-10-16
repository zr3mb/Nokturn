import QtQuick
import Nokturn

// Przyciski okna (minimalizuj / maksymalizuj / zamknij) dla okna bez ramki.
Row {
    id: root
    readonly property var win: Window.window
    readonly property bool maximized: win && win.visibility === Window.Maximized
    spacing: 2

    component WinButton: Item {
        id: btn
        property string icon: ""
        property bool danger: false
        signal clicked

        width: 44
        height: 34

        Rectangle {
            anchors.fill: parent
            radius: 9
            color: btn.danger ? "#b85c5c" : Qt.rgba(1, 1, 1, 0.08)
            opacity: m.containsMouse ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: 140 } }
        }
        Icon {
            anchors.centerIn: parent
            name: btn.icon
            size: 16
            color: btn.danger && m.containsMouse ? "#ffffff" : Theme.textDim
        }
        MouseArea {
            id: m
            anchors.fill: parent
            hoverEnabled: true
            onClicked: btn.clicked()
        }
    }

    WinButton {
        icon: "minimize"
        onClicked: root.win.showMinimized()
    }
    WinButton {
        icon: root.maximized ? "restore" : "maximize"
        onClicked: root.maximized ? root.win.showNormal() : root.win.showMaximized()
    }
    WinButton {
        icon: "close"
        danger: true
        onClicked: root.win.close()
    }
}
