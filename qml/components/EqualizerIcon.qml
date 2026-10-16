import QtQuick
import Nokturn

// Mała animowana ikonka "gra teraz" (trzy skaczące słupki).
Item {
    id: root
    property bool running: true
    property color color: Theme.accent

    implicitWidth: 15
    implicitHeight: 14

    Row {
        height: root.height
        spacing: 2

        Repeater {
            model: 3

            Rectangle {
                id: bar
                required property int index
                width: 3.5
                height: 4
                y: root.height - height
                radius: 1.5
                color: root.color

                SequentialAnimation on height {
                    running: root.running
                    loops: Animation.Infinite
                    NumberAnimation { to: root.height; duration: 280 + bar.index * 90; easing.type: Easing.InOutSine }
                    NumberAnimation { to: 4 + bar.index * 2; duration: 250 + bar.index * 60; easing.type: Easing.InOutSine }
                    NumberAnimation { to: root.height * 0.7; duration: 220 + bar.index * 40; easing.type: Easing.InOutSine }
                    NumberAnimation { to: 3; duration: 300; easing.type: Easing.InOutSine }
                }
            }
        }
    }
}
