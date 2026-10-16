import QtQuick
import Nokturn

// Track source badge: local file / YouTube Music / radio.
Rectangle {
    id: root
    property string source: "local"
    property bool showLabel: true

    readonly property var info: ({
        local:   { label: qsTr("Local"),       icon: "folder",     color: "#9fbfa8" },
        youtube: { label: "YouTube Music", icon: "playCircle", color: "#cf9d9d" },
        radio:   { label: qsTr("Radio"),       icon: "live",       color: "#cdb68e" },
        stream:  { label: qsTr("Stream"),      icon: "link",       color: "#93a8c3" }
    })[source] || { label: source, icon: "music", color: "#a19e98" }

    implicitWidth: row.implicitWidth + (showLabel ? 18 : 12)
    implicitHeight: 24
    radius: height / 2
    color: Theme.alpha(info.color, 0.12)
    border.width: 1
    border.color: Theme.alpha(info.color, 0.2)

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 6

        Icon {
            anchors.verticalCenter: parent.verticalCenter
            name: root.info.icon
            size: 13
            color: root.info.color
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            visible: root.showLabel
            text: root.info.label
            color: Qt.lighter(root.info.color, 1.12)
            font.pixelSize: 11
            font.weight: Font.DemiBold
        }
    }
}
