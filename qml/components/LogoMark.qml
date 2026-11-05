import QtQuick
import QtQuick.Shapes
import Everplay

// The logo (packaging/brand): a dark tile with the "e" and the disc in its middle, drawn as vectors so it
// stays sharp at any size. While music plays the disc breathes with it and, on the larger marks, the
// lines around it follow the spectrum.
Rectangle {
    id: root
    width: 34
    height: 34
    radius: Theme.r(11 * width / 34)
    gradient: Gradient {
        orientation: Gradient.Vertical
        GradientStop { position: 0; color: "#383838" }
        GradientStop { position: 1; color: "#171717" }
    }
    border.width: Math.max(1, width / 60)
    border.color: Qt.rgba(1, 1, 1, 0.06)

    property real phase: 0
    NumberAnimation on phase {
        from: 0; to: Math.PI * 2
        duration: 2600
        loops: Animation.Infinite
    }

    readonly property var spec: Player.spectrum
    readonly property real k: width / 1024     // the drawing is in the logo file's 1024 units
    readonly property color ink: "#efefef"

    // Cat theme: cat ears on top of the tile
    Shape {
        visible: Theme.cat
        z: -1
        width: root.width; height: 12 * root.width / 34
        y: -7 * root.width / 34
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: "transparent"
            fillColor: "#383838"
            scale: Qt.size(root.width / 34, root.width / 34)
            PathSvg { path: "M 2 14 L 4 0 L 15 9 Z M 32 14 L 30 0 L 19 9 Z" }
        }
        ShapePath {
            strokeColor: "transparent"
            fillColor: Theme.accent
            scale: Qt.size(root.width / 34, root.width / 34)
            PathSvg { path: "M 5.5 11 L 6.5 4.5 L 11.5 8.8 Z M 28.5 11 L 27.5 4.5 L 22.5 8.8 Z" }
        }
    }

    // The spectrum lines around the disc: from its middle outwards, so the disc covers their inner part
    Repeater {
        model: root.width >= 48 ? 36 : 0     // too fine to see on the small marks
        Rectangle {
            required property int index
            readonly property real level: Player.playing ? Math.min(1, (root.spec[index % 32] || 0) * 1.4)
                                                         : 0.35 + 0.25 * Math.sin(root.phase + index * 0.7)
            width: Math.max(1, 4 * root.k)
            height: (106 + 30 * level) * root.k
            radius: width / 2
            x: 518 * root.k - width / 2
            y: 520 * root.k - height
            color: "#9a9a9a"
            opacity: 0.85
            transform: Rotation { origin.x: width / 2; origin.y: height; angle: index * 10 }
        }
    }

    // The "e": one stroke from its lower end round the left and the top into the bar. ShapePath.scale
    // scales the outline only, so the stroke width is given in pixels.
    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            fillColor: "transparent"
            strokeColor: root.ink
            strokeWidth: 112 * root.k
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            scale: Qt.size(root.k, root.k)
            PathSvg { path: "M748.6 669.8 A275 275 0 1 1 746.0 366.2 L672 466" }
        }
    }

    // The disc
    Rectangle {
        readonly property real pulse: Player.playing ? Player.level * 0.08 : 0.015 * Math.sin(root.phase)
        width: 200 * root.k * (1 + pulse)
        height: width
        radius: width / 2
        x: 518 * root.k - width / 2
        y: 520 * root.k - height / 2
        color: root.ink
    }
}
