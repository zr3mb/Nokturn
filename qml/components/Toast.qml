import QtQuick
import QtQuick.Effects
import Nokturn

// Krótkie powiadomienie wysuwane nad paskiem odtwarzacza.
Item {
    id: root
    property real bottomOffset: 120
    property string text: ""
    property string icon: "check"
    property bool shown: false

    function show(t, i) {
        text = t
        icon = i || "check"
        shown = true
        timer.restart()
    }

    anchors.horizontalCenter: parent.horizontalCenter
    width: pill.width
    height: pill.height
    y: parent.height - bottomOffset - height + (shown ? 0 : 26)
    opacity: shown ? 1 : 0
    scale: shown ? 1 : 0.9
    visible: opacity > 0.01
    Behavior on y { NumberAnimation { duration: 460; easing.type: Easing.OutBack } }
    Behavior on opacity { NumberAnimation { duration: 260 } }
    Behavior on scale { NumberAnimation { duration: 460; easing.type: Easing.OutBack } }

    Timer {
        id: timer
        interval: 2400
        onTriggered: root.shown = false
    }

    Rectangle {
        id: pill
        width: row.implicitWidth + 30
        height: 46
        radius: 23
        color: Theme.surfaceHi
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.08)
        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowBlur: 1.0
            shadowOpacity: 0.55
            shadowVerticalOffset: 8
        }

        Row {
            id: row
            anchors.centerIn: parent
            spacing: 10

            Rectangle {
                width: 26; height: 26; radius: 13
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.accent
                Icon {
                    anchors.centerIn: parent
                    name: root.icon
                    size: 15
                    color: "#15161a"
                }
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: root.text
                color: Theme.text
                font.pixelSize: 14
                font.weight: Font.Medium
            }
        }
    }
}
