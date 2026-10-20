import QtQuick
import QtQuick.Shapes
import Nokturn

// Logo: a rounded tile with an accent gradient and "breathing" bars.
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

    // Femboy theme: cat ears on top of the tile
    Shape {
        visible: Theme.femboy
        z: -1
        width: root.width; height: 12
        y: -7
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: "transparent"
            fillColor: Qt.lighter(Theme.accent, 1.08)
            PathSvg { path: `M 2 14 L 4 0 L 15 9 Z M ${root.width - 2} 14 L ${root.width - 4} 0 L ${root.width - 15} 9 Z` }
        }
        ShapePath {
            strokeColor: "transparent"
            fillColor: Theme.alpha(Theme.onAccent, 0.45)
            PathSvg { path: `M 5.5 11 L 6.5 4.5 L 11.5 8.8 Z M ${root.width - 5.5} 11 L ${root.width - 6.5} 4.5 L ${root.width - 11.5} 8.8 Z` }
        }
    }

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
