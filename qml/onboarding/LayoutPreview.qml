import QtQuick
import QtQuick.Shapes
import Nokturn

// Animowana miniatura okna Nokturna do kreatora: pokazuje, jak wygląda dany wariant układu
// (panel boczny pełny / kompaktowy, panel "Teraz odtwarzane", kolor akcentu, żywe tło).
// Wszystko rysowane prostymi kształtami w jednostkach względnych (u = 1% szerokości).
Item {
    id: root
    property bool compact: false
    property bool showQueue: true
    property bool dynamicAccent: true
    property bool ambient: true
    property bool running: true

    implicitWidth: 240
    implicitHeight: width * 0.62

    readonly property real u: width / 100
    readonly property bool animate: running && visible

    // Kolejne "okładki" — w trybie dynamicznym akcent podąża za nimi
    readonly property var palettes: [
        ["#9fb5a8", "#3e4f5a"], ["#d9a07b", "#5b3a3f"], ["#8fa8d8", "#2f3c63"],
        ["#c9a6d6", "#4a3560"], ["#b5c98b", "#3a502f"]
    ]
    property int palette: 0
    property color accent: dynamicAccent ? palettes[palette][0] : "#9fb5a8"
    property color accent2: dynamicAccent ? palettes[palette][1] : "#4a5d6e"
    Behavior on accent { ColorAnimation { duration: 900; easing.type: Easing.InOutCubic } }
    Behavior on accent2 { ColorAnimation { duration: 900; easing.type: Easing.InOutCubic } }

    Timer {
        interval: 2600
        running: root.animate && root.dynamicAccent
        repeat: true
        onTriggered: root.palette = (root.palette + 1) % root.palettes.length
    }

    // Podświetlenie "przechodzi" po pozycjach panelu bocznego
    property int active: 0
    Timer {
        interval: 1400
        running: root.animate
        repeat: true
        onTriggered: root.active = (root.active + 1) % 5
    }

    property real progress: 0
    NumberAnimation on progress { from: 0; to: 1; duration: 12000; loops: Animation.Infinite; running: root.animate }
    property real phase: 0
    NumberAnimation on phase { from: 0; to: Math.PI * 2; duration: 1500; loops: Animation.Infinite; running: root.animate }

    readonly property color surface: Qt.rgba(1, 1, 1, 0.045)
    readonly property color line: Qt.rgba(1, 1, 1, 0.16)
    readonly property color lineDim: Qt.rgba(1, 1, 1, 0.08)

    Rectangle {
        id: frame
        anchors.fill: parent
        radius: 2.2 * root.u
        color: "#101114"
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.08)
        clip: true

        // ---- żywe tło: miękkie, pulsujące światło w kolorach akcentu ----
        Item {
            anchors.fill: parent
            opacity: root.ambient ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: 600 } }

            Shape {
                anchors.fill: parent
                preferredRendererType: Shape.CurveRenderer
                property real pulse: 0.75 + 0.25 * Math.sin(root.phase * 0.5)
                ShapePath {
                    strokeColor: "transparent"
                    fillGradient: RadialGradient {
                        centerX: frame.width * 0.22; centerY: frame.height * 0.2
                        focalX: centerX; focalY: centerY
                        centerRadius: frame.width * 0.55; focalRadius: 0
                        GradientStop { position: 0; color: Theme.alpha(root.accent, 0.34) }
                        GradientStop { position: 1; color: "transparent" }
                    }
                    PathRectangle { x: 0; y: 0; width: frame.width; height: frame.height }
                }
                ShapePath {
                    strokeColor: "transparent"
                    fillGradient: RadialGradient {
                        centerX: frame.width * 0.85; centerY: frame.height * 0.9
                        focalX: centerX; focalY: centerY
                        centerRadius: frame.width * 0.5; focalRadius: 0
                        GradientStop { position: 0; color: Theme.alpha(root.accent2, 0.5) }
                        GradientStop { position: 1; color: "transparent" }
                    }
                    PathRectangle { x: 0; y: 0; width: frame.width; height: frame.height }
                }
                opacity: pulse
            }
        }

        // ---- pasek tytułu ----
        Rectangle {
            x: 34 * root.u; y: 1.6 * root.u
            width: 32 * root.u; height: 3 * root.u
            radius: height / 2
            color: root.lineDim
        }
        Row {
            anchors { right: parent.right; rightMargin: 2 * root.u }
            y: 2.4 * root.u
            spacing: 1.4 * root.u
            Repeater {
                model: 3
                Rectangle { width: 1.4 * root.u; height: width; radius: width / 2; color: root.lineDim }
            }
        }

        // ---- panel boczny ----
        Rectangle {
            id: side
            x: 1.6 * root.u
            y: 6.4 * root.u
            width: root.compact ? 8.5 * root.u : 22 * root.u
            height: player.y - y - 1.2 * root.u
            radius: 1.8 * root.u
            color: root.surface
            clip: true
            Behavior on width { NumberAnimation { duration: 520; easing.type: Easing.OutQuint } }

            Rectangle {
                id: logo
                x: 2 * root.u; y: 2 * root.u
                width: 4.4 * root.u; height: width
                radius: 1.4 * root.u
                gradient: Gradient {
                    GradientStop { position: 0; color: Qt.lighter(root.accent, 1.08) }
                    GradientStop { position: 1; color: root.accent2 }
                }
            }
            Rectangle {
                x: logo.x + logo.width + 1.6 * root.u
                anchors.verticalCenter: logo.verticalCenter
                width: 9 * root.u; height: 1.6 * root.u; radius: height / 2
                color: root.line
                opacity: root.compact ? 0 : 1
                Behavior on opacity { NumberAnimation { duration: 260 } }
            }

            // podświetlenie aktywnej pozycji
            Rectangle {
                x: 1 * root.u
                y: 9.5 * root.u + root.active * 4.6 * root.u - 0.9 * root.u
                width: side.width - 2 * root.u
                height: 4 * root.u
                radius: 1.2 * root.u
                color: Qt.rgba(1, 1, 1, 0.08)
                Behavior on y { NumberAnimation { duration: 480; easing.type: Easing.OutQuint } }
                Rectangle {
                    width: 0.6 * root.u; height: parent.height * 0.6
                    anchors.verticalCenter: parent.verticalCenter
                    radius: width / 2
                    color: root.accent
                }
            }

            Repeater {
                model: 5
                Item {
                    required property int index
                    x: 2.4 * root.u
                    y: 9.5 * root.u + index * 4.6 * root.u
                    width: side.width - 4 * root.u
                    height: 2.2 * root.u
                    Rectangle {
                        width: 2.2 * root.u; height: width; radius: 0.6 * root.u
                        color: index === root.active ? root.accent : root.line
                        Behavior on color { ColorAnimation { duration: 300 } }
                    }
                    Rectangle {
                        x: 3.6 * root.u
                        anchors.verticalCenter: parent.verticalCenter
                        width: (8 + (index * 3) % 5) * root.u; height: 1.3 * root.u; radius: height / 2
                        color: index === root.active ? Qt.rgba(1, 1, 1, 0.5) : root.line
                        opacity: root.compact ? 0 : 1
                        Behavior on opacity { NumberAnimation { duration: 220 } }
                    }
                }
            }

            // playlisty (tylko w pełnym panelu)
            Column {
                x: 2.4 * root.u
                y: 34 * root.u
                spacing: 1.6 * root.u
                opacity: root.compact ? 0 : 1
                Behavior on opacity { NumberAnimation { duration: 220 } }
                Repeater {
                    model: 2
                    Row {
                        required property int index
                        spacing: 1.4 * root.u
                        Rectangle {
                            width: 3.4 * root.u; height: width; radius: 0.8 * root.u
                            color: root.palettes[(index + 2) % root.palettes.length][0]
                        }
                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 0.8 * root.u
                            Rectangle { width: 9 * root.u; height: 1.1 * root.u; radius: height / 2; color: root.line }
                            Rectangle { width: 6 * root.u; height: 0.9 * root.u; radius: height / 2; color: root.lineDim }
                        }
                    }
                }
            }
        }

        // ---- panel "Teraz odtwarzane" ----
        Rectangle {
            id: queue
            anchors { right: parent.right; rightMargin: root.showQueue ? 1.6 * root.u : -width }
            y: side.y
            width: 21 * root.u
            height: side.height
            radius: 1.8 * root.u
            color: root.surface
            opacity: root.showQueue ? 1 : 0
            Behavior on anchors.rightMargin { NumberAnimation { duration: 520; easing.type: Easing.OutQuint } }
            Behavior on opacity { NumberAnimation { duration: 300 } }

            Rectangle {
                id: qCover
                x: 2 * root.u; y: 2.4 * root.u
                width: parent.width - 4 * root.u; height: width
                radius: 1.4 * root.u
                gradient: Gradient {
                    GradientStop { position: 0; color: root.accent }
                    GradientStop { position: 1; color: root.accent2 }
                }
                scale: 1 + 0.02 * Math.sin(root.phase)
            }
            Column {
                x: 2 * root.u
                y: qCover.y + qCover.height + 1.8 * root.u
                spacing: 1 * root.u
                Rectangle { width: 12 * root.u; height: 1.5 * root.u; radius: height / 2; color: Qt.rgba(1, 1, 1, 0.5) }
                Rectangle { width: 8 * root.u; height: 1.1 * root.u; radius: height / 2; color: root.line }
            }
            Row {
                anchors { left: parent.left; leftMargin: 2 * root.u; bottom: parent.bottom; bottomMargin: 2 * root.u }
                spacing: 0.5 * root.u
                Repeater {
                    model: 12
                    Rectangle {
                        required property int index
                        anchors.bottom: parent.bottom
                        width: 0.9 * root.u
                        height: (1 + 2.6 * Math.abs(Math.sin(root.phase + index * 0.7))) * root.u
                        radius: width / 2
                        color: Theme.alpha(root.accent, 0.8)
                    }
                }
            }
        }

        // ---- treść ----
        Rectangle {
            id: content
            anchors {
                left: side.right; leftMargin: 1.2 * root.u
                right: root.showQueue ? queue.left : parent.right
                rightMargin: root.showQueue ? 1.2 * root.u : 1.6 * root.u
            }
            y: side.y
            height: side.height
            radius: 1.8 * root.u
            color: root.surface
            clip: true

            Rectangle {
                id: hero
                x: 2 * root.u; y: 2 * root.u
                width: parent.width - 4 * root.u
                height: parent.height * 0.42
                radius: 1.6 * root.u
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0; color: Theme.alpha(root.accent, 0.85) }
                    GradientStop { position: 1; color: Theme.alpha(root.accent2, 0.85) }
                }
                Column {
                    x: 2.4 * root.u
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 1.1 * root.u
                    Rectangle { width: 6 * root.u; height: 0.9 * root.u; radius: height / 2; color: Qt.rgba(1, 1, 1, 0.55) }
                    Rectangle { width: 16 * root.u; height: 2.2 * root.u; radius: height / 2; color: Qt.rgba(1, 1, 1, 0.9) }
                    Rectangle { width: 10 * root.u; height: 1.1 * root.u; radius: height / 2; color: Qt.rgba(1, 1, 1, 0.55) }
                    Rectangle { width: 4.4 * root.u; height: width; radius: width / 2; color: "#f4f0ea" }
                }
            }
            Flow {
                x: 2 * root.u
                y: hero.y + hero.height + 2 * root.u
                width: parent.width - 4 * root.u
                spacing: 1.6 * root.u
                Repeater {
                    model: 10
                    Column {
                        required property int index
                        spacing: 0.8 * root.u
                        Rectangle {
                            width: 7.4 * root.u; height: width; radius: 1 * root.u
                            gradient: Gradient {
                                GradientStop { position: 0; color: root.palettes[index % root.palettes.length][0] }
                                GradientStop { position: 1; color: root.palettes[index % root.palettes.length][1] }
                            }
                        }
                        Rectangle { width: 5.6 * root.u; height: 0.9 * root.u; radius: height / 2; color: root.line }
                    }
                }
            }
        }

        // ---- pasek odtwarzacza ----
        Item {
            id: player
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: 11 * root.u

            Rectangle {
                x: 2 * root.u
                anchors.verticalCenter: parent.verticalCenter
                width: 6.4 * root.u; height: width
                radius: 1 * root.u
                gradient: Gradient {
                    GradientStop { position: 0; color: root.accent }
                    GradientStop { position: 1; color: root.accent2 }
                }
            }
            Column {
                x: 10 * root.u
                anchors.verticalCenter: parent.verticalCenter
                spacing: 1 * root.u
                Rectangle { width: 10 * root.u; height: 1.4 * root.u; radius: height / 2; color: Qt.rgba(1, 1, 1, 0.5) }
                Rectangle { width: 7 * root.u; height: 1 * root.u; radius: height / 2; color: root.line }
            }
            Rectangle {
                id: playBtn
                anchors.horizontalCenter: parent.horizontalCenter
                y: 1.4 * root.u
                width: 4.6 * root.u; height: width
                radius: width / 2
                color: "#efebe5"
            }
            Rectangle {
                anchors { horizontalCenter: parent.horizontalCenter; top: playBtn.bottom; topMargin: 1.2 * root.u }
                width: 36 * root.u; height: 0.8 * root.u
                radius: height / 2
                color: root.lineDim
                Rectangle {
                    width: parent.width * root.progress
                    height: parent.height
                    radius: height / 2
                    color: root.accent
                }
            }
            Row {
                anchors { right: parent.right; rightMargin: 2.4 * root.u; verticalCenter: parent.verticalCenter }
                spacing: 1.6 * root.u
                Repeater {
                    model: 3
                    Rectangle { width: 2 * root.u; height: width; radius: 0.5 * root.u; color: root.line }
                }
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 8 * root.u; height: 0.8 * root.u; radius: height / 2
                    color: root.line
                }
            }
        }
    }
}
