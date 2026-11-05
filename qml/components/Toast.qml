import QtQuick
import QtQuick.Effects
import Everplay

// Short notification that slides in above the player bar.
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
    Behavior on y { NumberAnimation { duration: Theme.ms(460); easing.type: Easing.OutBack } }
    Behavior on opacity { NumberAnimation { duration: Theme.ms(260) } }
    Behavior on scale { NumberAnimation { duration: Theme.ms(460); easing.type: Easing.OutBack } }

    Timer {
        id: timer
        interval: 2400
        onTriggered: root.shown = false
    }

    Rectangle {
        id: pill
        width: row.implicitWidth + 28
        height: 48
        radius: 24
        color: Theme.alpha(Theme.surfaceHi, 0.94)
        border.width: 1
        border.color: Theme.alpha(Theme.accent, 0.3)
        layer.enabled: root.visible   // the shadow's textures are released between notifications
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
                width: 30; height: 30; radius: 15
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.accent
                Icon {
                    anchors.centerIn: parent
                    name: root.icon
                    size: 15
                    color: Theme.onAccent
                }
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: root.text
                color: Theme.text
                font.pixelSize: 14
                font.weight: Font.DemiBold
                rightPadding: 6
            }
        }
    }
}
