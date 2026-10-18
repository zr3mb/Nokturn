import QtQuick
import QtQuick.Effects
import QtQuick.Shapes
import Nokturn

// The blurred silhouette of a card's cover, rendered once and drawn under every card of a list
// (AlbumCard.shadow) — instead of a MultiEffect per card, which keeps six offscreen textures each.
// The blur is the cards' own shadow effect (blur 1.0, blurMax 32); each card applies the opacity and offset.
// Draws nothing itself: it's a texture for the cards.
ShaderEffectSource {
    id: root
    property real size: 180     // side of the cover
    property real radius: 14
    readonly property real padding: 32   // MultiEffect's automatic padding: blurMax on every side

    width: 0
    height: 0
    sourceItem: blurred
    sourceRect: Qt.rect(-padding, -padding, size + padding * 2, size + padding * 2)
    hideSource: true

    MultiEffect {
        id: blurred
        width: root.size
        height: root.size
        source: silhouette
        blurEnabled: true
        blur: 1.0
    }

    Shape {
        id: silhouette
        width: root.size
        height: root.size
        visible: false
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: "transparent"
            fillColor: "black"
            PathSvg { path: Theme.roundedRectPath(root.size, root.size, root.radius) }
        }
    }
}
