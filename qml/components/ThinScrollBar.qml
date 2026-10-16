import QtQuick
import QtQuick.Controls.Basic
import Nokturn

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
        Behavior on opacity { NumberAnimation { duration: 420 } }
        Behavior on implicitWidth { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
        Behavior on color { ColorAnimation { duration: 160 } }
    }
    background: Item {}
}
