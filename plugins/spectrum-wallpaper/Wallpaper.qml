import QtQuick
import QtQuick.Shapes
import Nokturn

// A wallpaper that moves with the music. Nokturn stretches this item over the whole window, behind
// the panels; the panels are translucent, so it shows through them.
Item {
    id: root

    // Set by Nokturn: this plugin's settings, files and id (see docs/plugins.md).
    property var plugin

    // Settings (Settings.qml writes them). `plugin.revision` makes the bindings follow changes.
    readonly property string style: plugin && plugin.revision >= 0 ? plugin.setting("style", "both") : "both"
    readonly property real intensity: plugin && plugin.revision >= 0 ? plugin.setting("intensity", 0.7) : 0.7

    // The music: overall loudness (0..1) and levels per frequency band, low to high.
    readonly property real level: Player.playing ? Player.level : 0
    readonly property var spectrum: Player.spectrum
    function band(from, to) {   // the average of a part of the spectrum (0..1 of its width)
        const s = spectrum
        if (!s || s.length === 0 || !Player.playing)
            return 0
        let sum = 0, n = 0
        for (let i = Math.floor(from * s.length); i < Math.ceil(to * s.length); ++i) { sum += s[i]; ++n }
        return n ? sum / n : 0
    }
    readonly property real bass: band(0, 0.12)
    readonly property real mids: band(0.12, 0.5)

    // Smoothed, so the light breathes instead of flickering.
    property real glow: 0
    Behavior on glow { NumberAnimation { duration: 180; easing.type: Easing.OutQuad } }
    onBassChanged: glow = Math.min(1, bass * 1.6) * intensity

    // Slow drift, a little faster while music plays.
    property real t: 0
    NumberAnimation on t {
        from: 0; to: Math.PI * 2
        duration: Player.playing ? 26000 : 60000
        loops: Animation.Infinite
        running: root.visible
    }

    // Three glowing shapes in the cover's colours (Theme.accent / accent2 follow the cover).
    component Glow: Shape {
        id: g
        property color color
        property real cx
        property real cy
        property real r
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: "transparent"
            fillGradient: RadialGradient {
                centerX: g.cx; centerY: g.cy; focalX: g.cx; focalY: g.cy
                centerRadius: g.r; focalRadius: 0
                GradientStop { position: 0; color: Qt.rgba(g.color.r, g.color.g, g.color.b, 0.55) }
                GradientStop { position: 0.45; color: Qt.rgba(g.color.r, g.color.g, g.color.b, 0.18) }
                GradientStop { position: 1; color: "transparent" }
            }
            PathRectangle { x: 0; y: 0; width: g.width; height: g.height }
        }
    }

    Item {
        anchors.fill: parent
        visible: root.style !== "bars"
        opacity: 0.35 + root.glow * 0.65

        Glow {
            color: Theme.accent
            cx: root.width * (0.25 + 0.12 * Math.sin(root.t))
            cy: root.height * (0.3 + 0.1 * Math.cos(root.t * 2))
            r: Math.max(root.width, root.height) * (0.42 + root.glow * 0.18)
        }
        Glow {
            color: Theme.accent2
            cx: root.width * (0.78 + 0.1 * Math.cos(root.t))
            cy: root.height * (0.62 + 0.14 * Math.sin(root.t * 3))
            r: Math.max(root.width, root.height) * (0.38 + root.mids * root.intensity * 0.25)
        }
        Glow {
            color: Qt.lighter(Theme.accent, 1.3)
            cx: root.width * (0.5 + 0.2 * Math.sin(root.t * 2 + 1))
            cy: root.height * (0.95 + 0.05 * Math.cos(root.t))
            r: Math.max(root.width, root.height) * (0.25 + root.glow * 0.2)
        }
    }

    // The spectrum along the bottom edge, mirrored: bass in the middle.
    Row {
        id: bars
        visible: root.style !== "glow"
        anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter }
        readonly property int count: 72
        spacing: 3
        height: root.height * 0.2
        Repeater {
            model: bars.count
            Rectangle {
                required property int index
                readonly property real value: {
                    const s = root.spectrum
                    if (!s || s.length === 0 || !Player.playing)
                        return 0
                    const half = (bars.count - 1) / 2
                    const at = Math.abs(index - half) / half
                    return Math.min(1, s[Math.floor(at * (s.length - 1))] * (0.6 + root.intensity))
                }
                width: (root.width - bars.spacing * (bars.count - 1)) / bars.count
                height: value * bars.height
                y: bars.height - height
                radius: Math.min(width / 2, 4)
                gradient: Gradient {
                    GradientStop { position: 0; color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.3) }
                    GradientStop { position: 1; color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.04) }
                }
                Behavior on height { NumberAnimation { duration: 90 } }
            }
        }
    }
}
