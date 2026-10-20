import QtQuick
import QtQuick.Shapes
import Nokturn

// Cover art: the real image (when there is one), and under it — or instead of it — a generative
// gradient in the album's colors with one of four graphic motifs.
Item {
    id: root
    property color colorA: "#a3bcab"
    property color colorB: "#3e594d"
    // The generated gradient leans toward the theme's own colors in the Cat theme.
    readonly property color fillA: Theme.cat ? Qt.tint(colorA, Theme.alpha(Theme.staticAccent, 0.7)) : colorA
    readonly property color fillB: Theme.cat ? Qt.tint(colorB, Theme.alpha(Theme.staticAccent2, 0.7)) : colorB
    property real radius: 10
    property int seed: 0
    property bool decorated: true
    property string source: ""
    property var mosaic: []   // four cover URLs — a 2×2 collage when there's no source (playlists)
    readonly property bool useMosaic: source === "" && mosaic && mosaic.length >= 4
    property int mosaicReady: 0
    // A new size of the same cover loads behind the one on screen (retainWhileLoading); a different
    // cover fades in once it's ready.
    property string shownSource: ""
    readonly property bool pictureShown: picture.status === Image.Ready
                                         || (picture.status === Image.Loading && source !== "" && shownSource === source)
    readonly property bool hasPicture: pictureShown || (useMosaic && mosaicReady > 0)
    // fillItem draws the texture at its natural size — we scale it to cover the shape
    // (like PreserveAspectCrop) and center it. Also applies to 16:9 music-video thumbnails.
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

    // Pictures are decoded at the size they're drawn at (device pixels, rounded up to a step), never
    // above the fixed sizes used before — a 42 px row cover doesn't keep a 192 px image in memory.
    function decodeSize(size, limit) {
        const px = Math.ceil(size * Screen.devicePixelRatio)
        for (const step of [48, 64, 96, 128, 192, 256, 384, 512])
            if (step >= px)
                return Math.min(step, limit)
        return limit
    }
    // Images give up their memory copy once they're on the GPU (main.cpp); after a GPU reset they're
    // loaded again under a new address.
    // Another window (the in-game overlay) keeps its own copies: an image already on the main window's
    // GPU crashes Qt's renderer when a second window draws it.
    property string cacheKey: ""
    function imageUrl(url) {
        if (url === "")
            return url
        const tag = [Theme.imageEpoch > 0 ? String(Theme.imageEpoch) : "", cacheKey].filter(x => x !== "").join("-")
        return tag !== "" ? url + "#" + tag : url
    }

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
                GradientStop { position: 0; color: Qt.lighter(root.fillA, 1.06) }
                GradientStop { position: 1; color: root.fillB }
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

    // The motif is only built when it shows (no picture) — covers appear in long lists, and four
    // curve-rendered shapes per cover add up.
    Loader {
        anchors.fill: parent
        active: root.decorated && !root.hasPicture
        sourceComponent: [motif0, motif1, motif2, motif3][root.variant]
    }

    // Motif 0 — concentric circles
    Component {
        id: motif0
        Shape {
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeColor: root.ink; strokeWidth: root.line; fillColor: "transparent"
                PathAngleArc { centerX: root.w * 0.64; centerY: root.h * 0.6; radiusX: root.w * 0.08; radiusY: root.w * 0.08; startAngle: 0; sweepAngle: 360 }
                PathAngleArc { centerX: root.w * 0.64; centerY: root.h * 0.6; radiusX: root.w * 0.17; radiusY: root.w * 0.17; startAngle: 0; sweepAngle: 360 }
                PathAngleArc { centerX: root.w * 0.64; centerY: root.h * 0.6; radiusX: root.w * 0.26; radiusY: root.w * 0.26; startAngle: 0; sweepAngle: 360 }
            }
        }
    }

    // Motif 1 — sun over the horizon
    Component {
        id: motif1
        Shape {
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
    }

    // Motif 2 — waves
    Component {
        id: motif2
        Shape {
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeColor: root.ink; strokeWidth: root.line * 1.2; fillColor: "transparent"; capStyle: ShapePath.RoundCap
                PathSvg { path: root.w > 0 ? root.waves() : "" }
            }
        }
    }

    // Motif 3 — overlapping circles
    Component {
        id: motif3
        Shape {
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
    }

    // The real cover fills the same rounded shape. Cropping makes the decoded short side match the
    // cover, so 16:9 thumbnails stay sharp at the smaller size.
    Image {
        id: picture
        readonly property int decodeSize: root.decodeSize(Math.max(root.w, root.h), root.w > 160 ? 512 : 192)
        width: root.w
        height: root.h
        visible: false
        source: root.imageUrl(root.source)
        sourceSize.width: decodeSize
        sourceSize.height: decodeSize
        fillMode: Image.PreserveAspectCrop
        retainWhileLoading: true
        asynchronous: true
        smooth: true
        mipmap: true
        onStatusChanged: {
            if (status === Image.Ready)
                root.shownSource = root.source
            else if (status !== Image.Loading)
                root.shownSource = ""
        }
    }

    // The collage (playlists): four tiles in the same rounded shape. Built only when used.
    // Keyed by the addresses — a list rebuilt with the same covers (a page re-evaluating) keeps the collage.
    readonly property string mosaicKey: useMosaic ? Array.from(mosaic).slice(0, 4).join("\n") : ""
    // The count goes back to zero before the new tiles exist (a cached image is ready at once).
    property var mosaicUrls: []
    function updateMosaic() {
        mosaicReady = 0
        mosaicUrls = mosaicKey === "" ? [] : mosaicKey.split("\n")
    }
    onMosaicKeyChanged: updateMosaic()
    Component.onCompleted: if (mosaicKey !== "") updateMosaic()   // rows by the thousand skip it
    Loader {
        anchors.fill: parent
        active: root.useMosaic
        sourceComponent: Item {
            Item {
                id: collage
                width: root.w
                height: root.h
                visible: false
                layer.enabled: true

                Rectangle {
                    anchors.fill: parent
                    color: root.fillB
                }
                Grid {
                    columns: 2
                    Repeater {
                        model: root.mosaicUrls
                        Image {
                            required property string modelData
                            readonly property int decodeSize: root.decodeSize(Math.max(root.w, root.h) / 2, root.w > 160 ? 256 : 96)
                            width: root.w / 2
                            height: root.h / 2
                            source: root.imageUrl(modelData)
                            fillMode: Image.PreserveAspectCrop
                            sourceSize.width: decodeSize
                            sourceSize.height: decodeSize
                            retainWhileLoading: true
                            asynchronous: true
                            smooth: true
                            mipmap: true
                            onStatusChanged: if (status === Image.Ready) root.mosaicReady += 1
                        }
                    }
                }
            }
            // Drawn through the cover's rounded shape (antialiased edges) — no mask texture.
            Shape {
                anchors.fill: parent
                opacity: root.mosaicReady > 0 ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 350 } }
                preferredRendererType: Shape.CurveRenderer
                ShapePath {
                    strokeColor: "transparent"
                    fillItem: collage
                    PathSvg { path: root.frame }
                }
            }
        }
    }

    Loader {
        anchors.fill: parent
        active: root.source !== ""
        sourceComponent: Shape {
            visible: root.pictureShown
            opacity: root.pictureShown ? 1 : 0
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
}