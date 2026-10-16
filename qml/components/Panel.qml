import QtQuick
import QtQuick.Effects
import Nokturn

// Półprzezroczysty, zaokrąglony panel — zawartość jest przycinana do zaokrągleń.
Item {
    id: root
    property real radius: 20
    property color tint: Theme.alpha(Theme.surface, 0.66)
    default property alias content: body.data

    Rectangle {
        anchors.fill: parent
        radius: root.radius
        color: root.tint
    }

    Item {
        id: body
        anchors.fill: parent
        layer.enabled: true
        layer.effect: MultiEffect {
            maskEnabled: true
            maskSource: mask
            maskThresholdMin: 0.5
            maskSpreadAtMin: 1.0
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: root.radius
        color: "transparent"
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.05)
    }

    Rectangle {
        id: mask
        anchors.fill: parent
        radius: root.radius
        visible: false
        layer.enabled: true
    }
}
