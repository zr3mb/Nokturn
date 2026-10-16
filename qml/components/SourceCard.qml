import QtQuick
import QtQuick.Layouts
import QtQuick.Shapes
import Nokturn

// Karta źródła muzyki z poświatą w kolorze źródła i rozwijaną treścią.
Item {
    id: root
    property var source: ({})
    property string status: source.status || "available"
    property string detail: source.detail || ""
    default property alias body: bodyCol.data
    readonly property color tint: source.color || Theme.accent
    readonly property string frame: Theme.roundedRectPath(width, height, 20)

    property real glow: hover.hovered ? 1 : 0
    Behavior on glow { NumberAnimation { duration: 400; easing.type: Easing.OutCubic } }

    property real animatedHeight: col.implicitHeight + 46
    Behavior on animatedHeight { NumberAnimation { duration: 520; easing.type: Easing.OutQuint } }

    Layout.fillWidth: true
    implicitHeight: animatedHeight
    clip: true

    HoverHandler { id: hover }

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: Theme.alpha(root.tint, 0.1 + root.glow * 0.2)
            strokeWidth: 1
            fillColor: Theme.alpha(Theme.text, 0.035)
            PathSvg { path: root.frame }
        }
        ShapePath {
            strokeColor: "transparent"
            fillGradient: RadialGradient {
                centerX: root.width * 0.08; centerY: 0
                focalX: root.width * 0.08; focalY: 0
                centerRadius: Math.max(root.width, root.height) * (0.75 + root.glow * 0.15)
                focalRadius: 0
                GradientStop { position: 0; color: Theme.alpha(root.tint, 0.11 + root.glow * 0.06) }
                GradientStop { position: 1; color: Theme.alpha(root.tint, 0) }
            }
            PathSvg { path: root.frame }
        }
    }

    ColumnLayout {
        id: col
        x: 22
        y: 22
        width: root.width - 44
        spacing: 18

        RowLayout {
            Layout.fillWidth: true
            spacing: 14

            Rectangle {
                Layout.preferredWidth: 50
                Layout.preferredHeight: 50
                Layout.alignment: Qt.AlignTop
                radius: 15
                color: Theme.alpha(root.tint, 0.15)
                border.width: 1
                border.color: Theme.alpha(root.tint, 0.22)
                scale: 1 + root.glow * 0.05
                rotation: root.glow * -4

                Icon {
                    anchors.centerIn: parent
                    name: root.source.icon || "music"
                    size: 24
                    color: root.tint
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                Text {
                    Layout.fillWidth: true
                    text: root.source.name || ""
                    color: Theme.text
                    font.family: Theme.fontDisplay
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }
                StatusPill {
                    status: root.status
                }
            }
        }

        Text {
            Layout.fillWidth: true
            text: root.source.description || ""
            color: Theme.textDim
            font.pixelSize: 13
            wrapMode: Text.WordWrap
            lineHeight: 1.15
        }

        Text {
            Layout.fillWidth: true
            visible: root.detail !== ""
            text: root.detail
            color: Theme.textFaint
            font.pixelSize: 12
            font.weight: Font.Medium
            font.letterSpacing: 0.4
        }

        ColumnLayout {
            id: bodyCol
            Layout.fillWidth: true
            spacing: 12
        }
    }
}
