import QtQuick
import QtQuick.Controls.Basic
import Nokturn

// Pole tekstowe z ikoną i poświatą przy fokusie.
TextField {
    id: root
    property string icon: ""
    property real radius: height / 2

    implicitHeight: 46
    leftPadding: icon !== "" ? 46 : 18
    rightPadding: 18
    color: Theme.text
    placeholderTextColor: Theme.textFaint
    selectionColor: Theme.alpha(Theme.accent, 0.4)
    selectedTextColor: Theme.text
    font.pixelSize: 14
    verticalAlignment: TextInput.AlignVCenter

    background: Item {
        Rectangle {
            anchors.fill: parent
            anchors.margins: -4
            radius: root.radius + 4
            color: "transparent"
            border.width: 4
            border.color: Theme.alpha(Theme.accent, 0.16)
            opacity: root.activeFocus ? 1 : 0
            scale: root.activeFocus ? 1 : 0.97
            Behavior on opacity { NumberAnimation { duration: 260 } }
            Behavior on scale { NumberAnimation { duration: 360; easing.type: Easing.OutBack } }
        }
        Rectangle {
            anchors.fill: parent
            radius: root.radius
            color: Theme.alpha(Theme.text, root.activeFocus ? 0.08 : root.hovered ? 0.07 : 0.05)
            border.width: 1
            border.color: root.activeFocus ? Theme.alpha(Theme.accent, 0.65) : Theme.alpha(Theme.text, 0.08)
            Behavior on color { ColorAnimation { duration: Theme.fast } }
            Behavior on border.color { ColorAnimation { duration: Theme.normal } }
        }
        Icon {
            x: 16
            anchors.verticalCenter: parent.verticalCenter
            visible: root.icon !== ""
            name: root.icon
            size: 20
            color: root.activeFocus ? Theme.accent : Theme.textDim
            Behavior on color { ColorAnimation { duration: Theme.normal } }
        }
    }
}
