import QtQuick
import Nokturn

// Source status as a tinted label (a spinner while connecting).
Rectangle {
    id: root
    property string status: "available"   // connected | disconnected | connecting | available
    readonly property var info: ({
        connected:    { label: qsTr("Connected"),     color: Theme.success },
        disconnected: { label: qsTr("Not connected"), color: Theme.danger },
        connecting:   { label: qsTr("Connecting…"),   color: Theme.warning },
        available:    { label: qsTr("Available"),     color: Theme.textDim }
    })[status] || { label: status, color: Theme.textDim }

    implicitWidth: row.implicitWidth + 22
    implicitHeight: 26
    radius: height / 2
    color: Theme.alpha(info.color, 0.1)
    Behavior on color { ColorAnimation { duration: Theme.normal } }

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 8

        Spinner {
            anchors.verticalCenter: parent.verticalCenter
            size: 11
            thickness: 2
            color: root.info.color
            visible: root.status === "connecting"
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.info.label
            color: root.info.color
            font.pixelSize: 12
            font.weight: Font.DemiBold
        }
    }
}
