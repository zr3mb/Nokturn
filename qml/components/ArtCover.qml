import QtQuick
import QtQuick.Shapes
import Nokturn

// Okładka: prawdziwy obrazek (gdy jest), a pod nim — i zamiast niego — generatywny
// gradient w kolorach albumu z jednym z czterech motywów graficznych.
Item {
    id: root
    property color colorA: "#a3bcab"
    property color colorB: "#3e594d"
    property real radius: 10
    property int seed: 0
    property bool decorated: true
    property string source: ""
    readonly property bool hasPicture: picture.status === Image.Ready
    // fillItem rysuje teksturę w jej naturalnym rozmiarze — skalujemy ją tak, by pokryła kształt
    // (jak PreserveAspectCrop), i centrujemy. Dotyczy też miniatur 16:9 z teledysków.
    readonly property real texW: Math.max(1, picture.implicitWidth)
    readonly property real texH: Math.max(1, picture.implicitHeight)
    readonly property real fillScale: Math.max(w / texW, h / texH)

    readonly property int variant: ((seed % 4) + 4) % 4
    readonly property real w: width
    readonly property real h: height
    readonly property string frame: Theme.roundedRectPath(w, h, radius)
    readonly property color ink: Qt.rgba(1, 1, 1, 0.22)
    readonly property real line: Math.max(1, w / 110)

    implicitWidth: 120
    implicitHeight: 120

    function waves() {
        let s = ""
        for (let k = 0; k < 4; ++k) {
            const y = h * (0.46 + k * 0.11)
            const a = h * 0.06
            s += `M ${w * 0.1} ${y} C ${w * 0.3} ${y - a}, ${w * 0.42} ${y + a}, ${w * 0.6} ${y} `
               + `S ${w * 0.82} ${y - a}, ${w * 0.9} ${y} `
        }
        return s
    }

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: "transparent"
            fillGradient: LinearGradient {
                x1: 0; y1: 0; x2: root.w; y2: root.h
                GradientStop { position: 0; color: Qt.lighter(root.colorA, 1.06) }
                GradientStop { position: 1; color: root.colorB }
            }
            PathSvg { path: root.frame }
        }
        ShapePath {
            strokeColor: "transparent"
            fillGradient: RadialGradient {
                centerX: root.w * 0.82; centerY: root.h * 0.12
                focalX: root.w * 0.82; focalY: root.h * 0.12
                centerRadius: root.w * 0.8; focalRadius: 0
                GradientStop { position: 0; color: Qt.rgba(1, 1, 1, 0.22) }
                GradientStop { position: 1; color: Qt.rgba(1, 1, 1, 0) }
            }
            PathSvg { path: root.frame }
        }
        ShapePath {
            strokeColor: "transparent"
            fillGradient: RadialGradient {
                centerX: root.w * 0.05; centerY: root.h
                focalX: root.w * 0.05; focalY: root.h
                centerRadius: root.w * 0.9; focalRadius: 0
                GradientStop { position: 0; color: Qt.rgba(0, 0, 0, 0.32) }
                GradientStop { position: 1; color: Qt.rgba(0, 0, 0, 0) }
            }
            PathSvg { path: root.frame }
        }
    }

    // Motyw 0 — koncentryczne okręgi
    Shape {
        anchors.fill: parent
        visible: root.decorated && !root.hasPicture && root.variant === 0
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: root.ink; strokeWidth: root.line; fillColor: "transparent"
            PathAngleArc { centerX: root.w * 0.64; centerY: root.h * 0.6; radiusX: root.w * 0.08; radiusY: root.w * 0.08; startAngle: 0; sweepAngle: 360 }
            PathAngleArc { centerX: root.w * 0.64; centerY: root.h * 0.6; radiusX: root.w * 0.17; radiusY: root.w * 0.17; startAngle: 0; sweepAngle: 360 }
            PathAngleArc { centerX: root.w * 0.64; centerY: root.h * 0.6; radiusX: root.w * 0.26; radiusY: root.w * 0.26; startAngle: 0; sweepAngle: 360 }
        }
    }

    // Motyw 1 — słońce nad horyzontem
    Shape {
        anchors.fill: parent
        visible: root.decorated && !root.hasPicture && root.variant === 1
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: "transparent"; fillColor: Qt.rgba(1, 1, 1, 0.17)
            PathAngleArc { centerX: root.w * 0.36; centerY: root.h * 0.4; radiusX: root.w * 0.17; radiusY: root.w * 0.17; startAngle: 0; sweepAngle: 360 }
        }
        ShapePath {
            strokeColor: root.ink; strokeWidth: root.line; fillColor: "transparent"; capStyle: ShapePath.RoundCap
            startX: root.w * 0.14; startY: root.h * 0.7
            PathLine { x: root.w * 0.86; y: root.h * 0.7 }
            PathMove { x: root.w * 0.24; y: root.h * 0.78 }
            PathLine { x: root.w * 0.76; y: root.h * 0.78 }
            PathMove { x: root.w * 0.34; y: root.h * 0.86 }
            PathLine { x: root.w * 0.66; y: root.h * 0.86 }
        }
    }

    // Motyw 2 — fale
    Shape {
        anchors.fill: parent
        visible: root.decorated && !root.hasPicture && root.variant === 2
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: root.ink; strokeWidth: root.line * 1.2; fillColor: "transparent"; capStyle: ShapePath.RoundCap
            PathSvg { path: root.w > 0 ? root.waves() : "" }
        }
    }

    // Motyw 3 — nachodzące koła
    Shape {
        anchors.fill: parent
        visible: root.decorated && !root.hasPicture && root.variant === 3
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: "transparent"; fillColor: Qt.rgba(1, 1, 1, 0.12)
            PathAngleArc { centerX: root.w * 0.6; centerY: root.h * 0.56; radiusX: root.w * 0.28; radiusY: root.w * 0.28; startAngle: 0; sweepAngle: 360 }
        }
        ShapePath {
            strokeColor: "transparent"; fillColor: Qt.rgba(0, 0, 0, 0.14)
            PathAngleArc { centerX: root.w * 0.4; centerY: root.h * 0.44; radiusX: root.w * 0.22; radiusY: root.w * 0.22; startAngle: 0; sweepAngle: 360 }
        }
        ShapePath {
            strokeColor: root.ink; strokeWidth: root.line; fillColor: "transparent"
            PathAngleArc { centerX: root.w * 0.4; centerY: root.h * 0.44; radiusX: root.w * 0.22; radiusY: root.w * 0.22; startAngle: 0; sweepAngle: 360 }
        }
    }

    // Prawdziwa okładka wypełnia ten sam zaokrąglony kształt
    Image {
        id: picture
        width: root.w
        height: root.h
        visible: false
        source: root.source
        sourceSize.width: root.w > 160 ? 512 : 192
        sourceSize.height: root.w > 160 ? 512 : 192
        asynchronous: true
        smooth: true
        mipmap: true
    }

    Shape {
        anchors.fill: parent
        visible: root.hasPicture
        opacity: root.hasPicture ? 1 : 0
        preferredRendererType: Shape.CurveRenderer
        Behavior on opacity { NumberAnimation { duration: 350 } }

        ShapePath {
            strokeColor: "transparent"
            fillItem: picture
            fillTransform: PlanarTransform.fromAffineMatrix(root.fillScale, 0, 0, root.fillScale,
                                                            (root.w - root.texW * root.fillScale) / 2,
                                                            (root.h - root.texH * root.fillScale) / 2)
            PathSvg { path: root.frame }
        }
    }
}