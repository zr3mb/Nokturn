import QtQuick
import Everplay

// Track source badge: local file / YouTube Music / Spotify / SoundCloud / radio.
Rectangle {
    id: root
    property string source: "local"
    property bool showLabel: true
    property bool plain: false   // icon and name only, without the pill (lists, where every row has one)

    readonly property var info: ({
        local:   { label: qsTr("Local"),       icon: "folder",     color: "#9fbfa8" },
        youtube: { label: "YouTube Music", icon: "playCircle", color: "#cf9d9d" },
        spotify: { label: "Spotify",       icon: "music",      color: "#8fc79a" },
        soundcloud: { label: "SoundCloud", icon: "wavecloud",      color: "#d9a27e" },
        radio:   { label: qsTr("Radio"),       icon: "live",       color: "#cdb68e" },
        stream:  { label: qsTr("Stream"),      icon: "link",       color: "#93a8c3" }
    })[source] || { label: source, icon: "music", color: "#a19e98" }

    implicitWidth: row.implicitWidth + (plain ? 0 : showLabel ? 18 : 12)
    implicitHeight: 24
    radius: height / 2
    color: plain ? "transparent" : Theme.alpha(info.color, 0.12)
    border.width: plain ? 0 : 1
    border.color: Theme.alpha(info.color, 0.2)

    Row {
        id: row
        anchors.centerIn: root.plain ? undefined : parent
        anchors.verticalCenter: root.plain ? parent.verticalCenter : undefined
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
            color: root.plain ? Theme.textDim : Qt.lighter(root.info.color, 1.12)
            font.pixelSize: root.plain ? 13 : 11
            font.weight: root.plain ? Font.Normal : Font.DemiBold
        }
    }
}
