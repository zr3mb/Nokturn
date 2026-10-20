import QtQuick
import QtQuick.Effects
import QtQuick.Shapes
import QtQuick.Window
import Nokturn

// What sits behind the panels: the chosen wallpaper (any theme) and, in the Femboy theme,
// a neon night city with hearts and sparkles. The translucent panels let it show through.
Item {
    id: root

    Image {
        id: wall
        anchors.fill: parent
        visible: Theme.wallpaper !== ""
        source: Theme.wallpaper === "" ? "" : Theme.imageEpoch > 0 ? Theme.wallpaper + "#" + Theme.imageEpoch : Theme.wallpaper
        // Loaded once at screen size, so resizing the window doesn't decode it again.
        sourceSize: Qt.size(Screen.width, Screen.height)
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: false
        opacity: status === Image.Ready ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 600 } }
    }

    Rectangle {
        anchors.fill: parent
        visible: wall.visible
        color: Theme.bg
        opacity: Theme.wallpaperDim
    }

    Loader {
        anchors.fill: parent
        active: Theme.femboy
        sourceComponent: femboyDecor
    }

    Component {
        id: femboyDecor

        Item {
            id: decor
            readonly property bool plain: Theme.wallpaper === ""

            // Purple haze at the top, pink light rising from the city
            Rectangle {
                anchors.fill: parent
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Theme.alpha(Theme.accent2, 0.16) }
                    GradientStop { position: 0.5; color: "transparent" }
                    GradientStop { position: 1.0; color: Theme.alpha(Theme.accent, 0.13) }
                }
            }

            // Skyline with lit windows, drawn once at 1600×260 and stretched to the window's width
            Item {
                id: city
                visible: decor.plain
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                height: 260
                opacity: 0.75

                readonly property var paths: {
                    let seed = 7
                    const rnd = () => { seed = (seed * 16807) % 2147483647; return (seed - 1) / 2147483646 }
                    let towers = "", lights = ""
                    let x = 0
                    while (x < 1600) {
                        const w = 46 + rnd() * 70
                        const h = 70 + rnd() * 170
                        towers += `M ${x} 260 V ${260 - h} H ${x + w - 4} V 260 Z `
                        for (let wy = 260 - h + 12; wy < 250; wy += 14)
                            for (let wx = x + 8; wx < x + w - 14; wx += 12)
                                if (rnd() < 0.12)
                                    lights += `M ${wx} ${wy} h 5 v 7 h -5 Z `
                        x += w
                    }
                    return { towers: towers, lights: lights }
                }

                Shape {
                    width: 1600; height: 260
                    transform: Scale { xScale: city.width / 1600 }
                    ShapePath {
                        strokeColor: "transparent"
                        fillColor: Qt.darker(Theme.bg, 1.6)
                        PathSvg { path: city.paths.towers }
                    }
                    ShapePath {
                        strokeColor: "transparent"
                        fillColor: Theme.alpha(Theme.accent, 0.3)
                        PathSvg { path: city.paths.lights }
                    }
                }
            }

            // Big neon heart, slowly breathing
            Item {
                id: neon
                x: parent.width * 0.62
                y: parent.height * 0.12
                width: 300; height: 300
                opacity: decor.plain ? 0.5 : 0.28
                SequentialAnimation on scale {
                    loops: Animation.Infinite
                    NumberAnimation { from: 1; to: 1.05; duration: 3200; easing.type: Easing.InOutSine }
                    NumberAnimation { from: 1.05; to: 1; duration: 3200; easing.type: Easing.InOutSine }
                }

                Icon {
                    id: heart
                    anchors.fill: parent
                    name: "heartOutline"
                    size: 300
                    color: Theme.accent
                    layer.enabled: true
                    layer.effect: MultiEffect {
                        shadowEnabled: true
                        shadowColor: Theme.accent
                        shadowBlur: 1.0
                        shadowOpacity: 1.0
                        shadowHorizontalOffset: 0
                        shadowVerticalOffset: 0
                        blurMax: 48
                    }
                }
            }

            // Hearts, sparkles and a cat scattered around the window
            Repeater {
                model: [
                    { x: 0.155, y: 0.19, s: 18, i: "heart", r: -12 },
                    { x: 0.16, y: 0.46, s: 30, i: "heartOutline", r: 10 },
                    { x: 0.05, y: 0.78, s: 18, i: "sparkle", r: 0 },
                    { x: 0.30, y: 0.06, s: 16, i: "sparkle", r: 0 },
                    { x: 0.45, y: 0.30, s: 26, i: "heartOutline", r: -8 },
                    { x: 0.55, y: 0.62, s: 20, i: "sparkle", r: 0 },
                    { x: 0.38, y: 0.72, s: 34, i: "cat", r: 6 },
                    { x: 0.70, y: 0.80, s: 22, i: "heart", r: 14 },
                    { x: 0.86, y: 0.08, s: 18, i: "sparkle", r: 0 },
                    { x: 0.93, y: 0.40, s: 28, i: "heartOutline", r: 12 },
                    { x: 0.82, y: 0.58, s: 16, i: "sparkle", r: 0 },
                    { x: 0.97, y: 0.86, s: 24, i: "heart", r: -10 }
                ]
                Icon {
                    required property var modelData
                    required property int index
                    x: decor.width * modelData.x
                    y: decor.height * modelData.y + drift
                    property real drift: 0
                    size: modelData.s
                    name: modelData.i
                    rotation: modelData.r
                    color: Theme.accent
                    opacity: 0.32
                    SequentialAnimation on drift {
                        loops: Animation.Infinite
                        PauseAnimation { duration: index * 370 }
                        NumberAnimation { from: 0; to: -10; duration: 4200 + index * 230; easing.type: Easing.InOutSine }
                        NumberAnimation { from: -10; to: 0; duration: 4200 + index * 230; easing.type: Easing.InOutSine }
                    }
                }
            }
        }
    }
}
