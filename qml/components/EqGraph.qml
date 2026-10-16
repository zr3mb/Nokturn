import QtQuick
import QtQuick.Shapes
import Nokturn

// Equalizer graph: frequency/dB grid, spectrum of the playing audio, the combined curve,
// the selected band's curve and draggable band points.
//  - drag: frequency (horizontal) and gain (vertical)
//  - mouse wheel: bandwidth (Q)
//  - double-click: resets the gain, right button: enables / disables the band
Item {
    id: g
    property var colors: []
    property int selected: 0
    property real range: 30
    signal selectRequested(int index)

    readonly property var bands: Equalizer.bands
    readonly property var curve: Equalizer.curve
    readonly property var bandCurve: Equalizer.bandCurves[selected] || []

    readonly property real padL: 46
    readonly property real padR: 18
    readonly property real padT: 18
    readonly property real padB: 32
    readonly property real plotW: Math.max(1, width - padL - padR)
    readonly property real plotH: Math.max(1, height - padT - padB)

    function xOf(f) { return padL + plotW * Math.log(f / 20) / Math.log(1000) }
    function fOf(x) { return 20 * Math.pow(1000, Math.max(0, Math.min(1, (x - padL) / plotW))) }
    function yOf(db) { return padT + plotH / 2 - Math.max(-range, Math.min(range, db)) / range * plotH / 2 }
    function dbOf(y) { return Math.max(-range, Math.min(range, (padT + plotH / 2 - y) / (plotH / 2) * range)) }
    function fmtFreq(f) {
        if (f >= 1000) return (f / 1000).toFixed(f >= 10000 ? 1 : 2).replace(/\.?0+$/, "") + " kHz"
        return Math.round(f) + " Hz"
    }
    function linePoints(values) {
        const pts = []
        const n = values.length
        for (let i = 0; i < n; ++i)
            pts.push(Qt.point(padL + plotW * i / (n - 1), yOf(values[i])))
        return pts
    }
    function filled(pts) {
        return [Qt.point(padL, yOf(0))].concat(pts).concat([Qt.point(padL + plotW, yOf(0))])
    }
    // SVG paths from points — PathSvg reliably refreshes on every change.
    function svg(pts, close) {
        if (pts.length === 0) return ""
        let s = "M " + pts[0].x.toFixed(1) + " " + pts[0].y.toFixed(1)
        for (let i = 1; i < pts.length; ++i)
            s += " L " + pts[i].x.toFixed(1) + " " + pts[i].y.toFixed(1)
        return close ? s + " Z" : s
    }

    readonly property var curvePts: linePoints(curve)
    readonly property var bandPts: linePoints(bandCurve)
    // Spectrum: 48 bands from the analyzer (40 Hz – 16 kHz, logarithmic spacing)
    readonly property var spectrumPts: {
        const s = Player.spectrum
        const pts = [Qt.point(xOf(40), padT + plotH)]
        for (let i = 0; i < s.length; ++i) {
            const f = 40 * Math.pow(400, i / (s.length - 1))
            pts.push(Qt.point(xOf(f), padT + plotH - s[i] * plotH * 0.92))
        }
        pts.push(Qt.point(xOf(16000), padT + plotH))
        return pts
    }

    Rectangle {
        anchors.fill: parent
        radius: 20
        color: Qt.rgba(0, 0, 0, 0.22)
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.06)
    }

    // ---- grid ---------------------------------------------------------------------
    Repeater {
        model: [24, 18, 12, 6, 0, -6, -12, -18, -24]
        Item {
            required property var modelData
            Rectangle {
                x: g.padL
                y: g.yOf(parent.modelData)
                width: g.plotW
                height: 1
                color: parent.modelData === 0 ? Qt.rgba(1, 1, 1, 0.16) : Qt.rgba(1, 1, 1, 0.05)
            }
            Text {
                x: 6
                width: g.padL - 12
                y: g.yOf(parent.modelData) - height / 2
                horizontalAlignment: Text.AlignRight
                text: (parent.modelData > 0 ? "+" : "") + parent.modelData
                color: Theme.textFaint
                font.pixelSize: 10
                font.features: { "tnum": 1 }
            }
        }
    }
    Repeater {
        model: [20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000]
        Item {
            required property var modelData
            Rectangle {
                x: g.xOf(parent.modelData)
                y: g.padT
                width: 1
                height: g.plotH
                color: Qt.rgba(1, 1, 1, 0.05)
            }
            Text {
                x: Math.min(g.width - width - 4, Math.max(g.padL - 6, g.xOf(parent.modelData) - width / 2))
                y: g.padT + g.plotH + 8
                text: parent.modelData >= 1000 ? (parent.modelData / 1000) + "k" : parent.modelData
                color: Theme.textFaint
                font.pixelSize: 10
            }
        }
    }

    // ---- spectrum and curves -------------------------------------------------------------
    Item {
        x: g.padL
        y: g.padT
        width: g.plotW
        height: g.plotH
        clip: true

        // The audio spectrum (after EQ) changes every frame — the simple geometry renderer is
        // much cheaper here than the smoothing CurveRenderer.
        Shape {
            x: -g.padL
            y: -g.padT
            width: g.width
            height: g.height

            ShapePath {
                strokeColor: "transparent"
                fillGradient: LinearGradient {
                    x1: 0; y1: g.padT; x2: 0; y2: g.padT + g.plotH
                    GradientStop { position: 0; color: Theme.alpha(Theme.text, 0.14) }
                    GradientStop { position: 1; color: Theme.alpha(Theme.text, 0.02) }
                }
                PathSvg { path: g.svg(g.spectrumPts, true) }
            }
        }

        Shape {
            x: -g.padL
            y: -g.padT
            width: g.width
            height: g.height
            preferredRendererType: Shape.CurveRenderer

            // selected band
            ShapePath {
                strokeColor: Theme.alpha(g.colors[g.selected] || Theme.accent, 0.7)
                strokeWidth: 1.5
                fillColor: Theme.alpha(g.colors[g.selected] || Theme.accent, 0.16)
                PathSvg { path: g.svg(g.filled(g.bandPts), true) }
            }

            // combined curve
            ShapePath {
                strokeColor: "transparent"
                fillGradient: LinearGradient {
                    x1: 0; y1: g.padT; x2: 0; y2: g.padT + g.plotH
                    GradientStop { position: 0; color: Theme.alpha(Theme.accent, 0.32) }
                    GradientStop { position: 0.5; color: Theme.alpha(Theme.accent, 0.06) }
                    GradientStop { position: 1; color: Theme.alpha(Theme.accent, 0.32) }
                }
                PathSvg { path: g.svg(g.filled(g.curvePts), true) }
            }
            ShapePath {
                strokeColor: Equalizer.enabled ? Theme.text : Theme.textFaint
                strokeWidth: 2.4
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin
                PathSvg { path: g.svg(g.curvePts, false) }
            }
        }
    }

    // ---- band points ----------------------------------------------------------------
    Repeater {
        model: 7   // fixed count — delegates are not recreated while dragging

        Item {
            id: node
            required property int index
            readonly property var band: g.bands[index]
            readonly property bool sel: index === g.selected
            readonly property color col: g.colors[index] || Theme.accent
            readonly property bool active: mouse.pressed || mouse.containsMouse

            width: 26
            height: 26
            x: g.xOf(band.freq) - width / 2
            y: (band.usesGain ? g.yOf(band.gain) : g.yOf(0)) - height / 2
            z: sel ? 2 : 1

            Rectangle {
                anchors.centerIn: parent
                width: node.sel ? 44 : 26
                height: width
                radius: width / 2
                color: Theme.alpha(node.col, 0.22)
                opacity: node.sel ? 1 : 0
                Behavior on width { NumberAnimation { duration: 260; easing.type: Easing.OutBack } }
                Behavior on opacity { NumberAnimation { duration: 200 } }
            }
            Rectangle {
                anchors.fill: parent
                radius: width / 2
                color: node.band.enabled ? node.col : Theme.surface
                border.width: 2
                border.color: node.col
                opacity: node.band.enabled ? 1 : 0.6
                scale: mouse.pressed ? 1.18 : mouse.containsMouse ? 1.08 : 1
                Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutBack } }

                Text {
                    anchors.centerIn: parent
                    text: node.index + 1
                    color: node.band.enabled ? "#15161a" : node.col
                    font.pixelSize: 12
                    font.weight: Font.Bold
                }
            }

            // Value readout above the point
            Rectangle {
                visible: node.active
                width: readout.implicitWidth + 16
                height: 24
                radius: 7
                x: (node.width - width) / 2
                y: node.y > 40 ? -height - 10 : node.height + 10
                color: Theme.surfaceHi
                border.color: Theme.alpha(node.col, 0.5)
                Text {
                    id: readout
                    anchors.centerIn: parent
                    text: g.fmtFreq(node.band.freq)
                          + (node.band.usesGain ? "  ·  " + (node.band.gain > 0 ? "+" : "") + node.band.gain.toFixed(1) + " dB" : "")
                          + "  ·  Q " + node.band.q.toFixed(2)
                    color: Theme.text
                    font.pixelSize: 11
                    font.features: { "tnum": 1 }
                }
            }

            MouseArea {
                id: mouse
                anchors.fill: parent
                anchors.margins: -8
                hoverEnabled: true
                preventStealing: true
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor

                onPressed: mouse => {
                    g.selectRequested(node.index)
                    if (mouse.button === Qt.RightButton)
                        Equalizer.setBandEnabled(node.index, !node.band.enabled)
                }
                onPositionChanged: mouse => {
                    if (!pressed || (pressedButtons & Qt.RightButton))
                        return
                    const p = mapToItem(g, mouse.x, mouse.y)
                    Equalizer.setBandFreq(node.index, g.fOf(p.x))
                    if (node.band.usesGain)
                        Equalizer.setBandGain(node.index, Math.round(g.dbOf(p.y) * 10) / 10)
                }
                onDoubleClicked: Equalizer.setBandGain(node.index, 0)
                onWheel: wheel => {
                    g.selectRequested(node.index)
                    Equalizer.setBandQ(node.index, node.band.q * Math.pow(1.12, wheel.angleDelta.y / 120))
                }
            }
        }
    }
}
