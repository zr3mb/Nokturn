import QtQuick
import Nokturn

// Słupkowy wizualizer zasilany widmem z PlayerController.
Item {
    id: root
    property int bars: 32
    property real barWidth: 3
    property real spacing: 2
    property real minHeight: 2
    property real gain: 1.0
    property bool centered: false     // słupki rosną symetrycznie od środka w pionie
    property bool symmetric: false    // bas na środku, wysokie częstotliwości na brzegach
    property color colorTop: Theme.accent
    property color colorBottom: Theme.alpha(Theme.accent, 0.35)
    readonly property var spec: Player.spectrum

    implicitWidth: bars * barWidth + (bars - 1) * spacing
    implicitHeight: 32

    Row {
        height: root.height
        spacing: root.spacing

        Repeater {
            model: root.bars

            Rectangle {
                required property int index
                readonly property real value: {
                    const s = root.spec
                    if (!s || s.length === 0) return 0
                    const half = (root.bars - 1) / 2
                    const t = root.symmetric
                            ? Math.abs(index - half) / Math.max(1, half)
                            : index / Math.max(1, root.bars - 1)
                    return Math.min(1, s[Math.floor(t * (s.length - 1))] * root.gain)
                }
                width: root.barWidth
                height: Math.max(root.minHeight, value * root.height)
                y: root.centered ? (root.height - height) / 2 : root.height - height
                radius: Math.min(width / 2, 3)
                gradient: Gradient {
                    GradientStop { position: 0; color: root.colorTop }
                    GradientStop { position: 1; color: root.colorBottom }
                }
            }
        }
    }
}
