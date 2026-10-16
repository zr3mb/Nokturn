import QtQuick
import Nokturn

// Logo: zaokrąglony kafelek z gradientem akcentu i "oddychającymi" słupkami.
Rectangle {
    id: root
    width: 34
    height: 34
    radius: 11
    gradient: Gradient {
        orientation: Gradient.Vertical
        GradientStop { position: 0; color: Qt.lighter(Theme.accent, 1.08) }
        GradientStop { position: 1; color: Qt.darker(Theme.accent2, 1.05) }
    }

    property real phase: 0
    NumberAnimation on phase {
        from: 0; to: Math.PI * 2
        duration: 2600
        loops: Animation.Infinite
    }

    readonly property var spec: Player.spectrum

    Row {
        anchors.centerIn: parent
        height: 18
        spacing: 3

        Repeater {
            model: 4
            Rectangle {
                required property int index
                width: 3
                radius: 1.5
                color: "#f4f0ea"
                height: Player.playing
                        ? 4 + 14 * Math.min(1, (root.spec[index * 7] || 0) * 1.3)
                        : 6 + 4 * Math.sin(root.phase + index * 0.9)
                y: (18 - height) / 2
            }
        }
    }
}
