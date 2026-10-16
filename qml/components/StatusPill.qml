import QtQuick
import Nokturn

// Source status with an animated dot (pulses when connected).
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

        Item {
            width: 8; height: 8
            anchors.verticalCenter: parent.verticalCenter

            Rectangle {
                id: ping
                anchors.centerIn: parent
                width: 8; height: 8; radius: 4
                color: root.info.color
                visible: root.status === "connected"
                SequentialAnimation on scale {
                    running: ping.visible
                    loops: Animation.Infinite
                    NumberAnimation { from: 1; to: 2.6; duration: 1400; easing.type: Easing.OutCubic }
                    PauseAnimation { duration: 600 }
                }
                SequentialAnimation on opacity {
                    running: ping.visible
                    loops: Animation.Infinite
                    NumberAnimation { from: 0.6; to: 0; duration: 1400; easing.type: Easing.OutCubic }
                    PauseAnimation { duration: 600 }
                }
            }
            Rectangle {
                anchors.centerIn: parent
                width: 8; height: 8; radius: 4
                color: root.info.color
                visible: root.status !== "connecting"
            }
            Spinner {
                anchors.centerIn: parent
                size: 11
                thickness: 2
                color: root.info.color
                visible: root.status === "connecting"
            }
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
