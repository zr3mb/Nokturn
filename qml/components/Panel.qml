import QtQuick
import QtQuick.Shapes
import Nokturn

// Translucent rounded panel — content is clipped to the rounded corners.
Item {
    id: root
    property real radius: 20
    property color tint: Theme.alpha(Theme.surface, 0.66)
    // Off for panels whose content never reaches the corners: a plain rectangular clip then does the
    // same without an offscreen copy of the panel.
    property bool roundedClip: true
    default property alias content: body.data

    Rectangle {
        anchors.fill: parent
        radius: root.radius
        color: root.tint
    }

    Item {
        id: body
        anchors.fill: parent
        clip: !root.roundedClip
        layer.enabled: root.roundedClip
        // The content is drawn through a rounded shape (antialiased edges) — no second, full-size mask texture.
        layer.effect: Shape {
            property var source
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeColor: "transparent"
                fillItem: source
                PathSvg { path: Theme.roundedRectPath(root.width, root.height, root.radius) }
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: root.radius
        color: "transparent"
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.05)
    }
}
