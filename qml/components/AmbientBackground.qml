import QtQuick
import QtQuick.Shapes
import Nokturn

// Żywe tło: miękkie, dryfujące plamy światła w kolorach bieżącej okładki,
// lekko pulsujące w rytm muzyki.
Item {
    id: root
    property real boost: 1.0
    readonly property real pulse: Player.level

    component Glow: Shape {
        id: glow
        property color tint: "white"
        property real cx: 0.5
        property real cy: 0.5
        property real spread: 0.5
        property real strength: 0.2

        anchors.fill: parent

        ShapePath {
            strokeColor: "transparent"
            fillGradient: RadialGradient {
                centerX: glow.cx * glow.width
                centerY: glow.cy * glow.height
                focalX: glow.cx * glow.width
                focalY: glow.cy * glow.height
                centerRadius: glow.spread * Math.max(glow.width, glow.height)
                focalRadius: 0
                GradientStop { position: 0.0; color: Qt.rgba(glow.tint.r, glow.tint.g, glow.tint.b, glow.strength) }
                GradientStop { position: 0.45; color: Qt.rgba(glow.tint.r, glow.tint.g, glow.tint.b, glow.strength * 0.42) }
                GradientStop { position: 1.0; color: Qt.rgba(glow.tint.r, glow.tint.g, glow.tint.b, 0) }
            }
            startX: 0; startY: 0
            PathLine { x: glow.width; y: 0 }
            PathLine { x: glow.width; y: glow.height }
            PathLine { x: 0; y: glow.height }
            PathLine { x: 0; y: 0 }
        }
    }

    Glow {
        tint: Theme.accent
        strength: (0.17 + root.pulse * 0.09) * root.boost
        spread: 0.55 + root.pulse * 0.05
        SequentialAnimation on cx {
            loops: Animation.Infinite
            NumberAnimation { from: 0.1; to: 0.34; duration: 17000; easing.type: Easing.InOutSine }
            NumberAnimation { from: 0.34; to: 0.1; duration: 17000; easing.type: Easing.InOutSine }
        }
        SequentialAnimation on cy {
            loops: Animation.Infinite
            NumberAnimation { from: 0.02; to: 0.28; duration: 23000; easing.type: Easing.InOutSine }
            NumberAnimation { from: 0.28; to: 0.02; duration: 23000; easing.type: Easing.InOutSine }
        }
    }

    Glow {
        tint: Qt.lighter(Theme.accent2, 1.5)
        strength: 0.16 * root.boost
        spread: 0.6
        SequentialAnimation on cx {
            loops: Animation.Infinite
            NumberAnimation { from: 0.92; to: 0.66; duration: 21000; easing.type: Easing.InOutSine }
            NumberAnimation { from: 0.66; to: 0.92; duration: 21000; easing.type: Easing.InOutSine }
        }
        SequentialAnimation on cy {
            loops: Animation.Infinite
            NumberAnimation { from: 0.95; to: 0.68; duration: 19000; easing.type: Easing.InOutSine }
            NumberAnimation { from: 0.68; to: 0.95; duration: 19000; easing.type: Easing.InOutSine }
        }
    }

    Glow {
        tint: Qt.tint(Theme.accent, Qt.rgba(Theme.accent2.r, Theme.accent2.g, Theme.accent2.b, 0.5))
        strength: 0.09 * root.boost
        spread: 0.42
        SequentialAnimation on cx {
            loops: Animation.Infinite
            NumberAnimation { from: 0.5; to: 0.78; duration: 27000; easing.type: Easing.InOutSine }
            NumberAnimation { from: 0.78; to: 0.5; duration: 27000; easing.type: Easing.InOutSine }
        }
        SequentialAnimation on cy {
            loops: Animation.Infinite
            NumberAnimation { from: 0.35; to: 0.6; duration: 15000; easing.type: Easing.InOutSine }
            NumberAnimation { from: 0.6; to: 0.35; duration: 15000; easing.type: Easing.InOutSine }
        }
    }

    // Winieta — przyciemnia dół, żeby pasek odtwarzacza był czytelny
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#00000000" }
            GradientStop { position: 0.7; color: "#00000000" }
            GradientStop { position: 1.0; color: "#59000000" }
        }
    }
}
