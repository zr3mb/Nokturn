import QtQuick
import QtQuick.Controls.Basic
import Everplay

ScrollBar {
    id: control
    policy: ScrollBar.AsNeeded
    minimumSize: 0.08
    padding: 3

    contentItem: Rectangle {
        implicitWidth: control.hovered || control.pressed ? 8 : 5
        radius: width / 2
        color: Theme.alpha(Theme.text, control.pressed ? 0.36 : control.hovered ? 0.26 : 0.15)
        opacity: control.active || control.hovered ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Theme.ms(420) } }
        Behavior on implicitWidth { NumberAnimation { duration: Theme.ms(160); easing.type: Easing.OutCubic } }
        Behavior on color { ColorAnimation { duration: Theme.ms(160) } }
    }
    background: Item {}
}
