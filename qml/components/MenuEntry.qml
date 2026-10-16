import QtQuick
import Nokturn

// One row of a small popup menu: icon + label (the track menu, the playlist cover menu).
Item {
    id: entry
    property string icon: ""
    property string text: ""
    signal triggered

    implicitWidth: label.implicitWidth + 64
    implicitHeight: 40

    Rectangle {
        anchors.fill: parent
        radius: 9
        color: Theme.alpha(Theme.text, area.containsMouse ? 0.07 : 0)
        Behavior on color { ColorAnimation { duration: Theme.fast } }
    }
    Icon {
        x: 12
        anchors.verticalCenter: parent.verticalCenter
        name: entry.icon
        size: 18
        color: area.containsMouse ? Theme.accent : Theme.textDim
    }
    Text {
        id: label
        x: 42
        anchors.verticalCenter: parent.verticalCenter
        text: entry.text
        color: Theme.text
        font.pixelSize: 14
    }
    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: entry.triggered()
    }
}
