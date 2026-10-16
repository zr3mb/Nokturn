import QtQuick
import QtQuick.Shapes
import Nokturn

// Wektorowa ikona z Icons.paths — skaluje się bez utraty jakości i przyjmuje dowolny kolor.
Item {
    id: root
    property string name: ""
    property color color: Theme.text
    property real size: 20

    implicitWidth: size
    implicitHeight: size

    Shape {
        width: 24
        height: 24
        scale: root.size / 24
        transformOrigin: Item.TopLeft
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            fillColor: root.color
            strokeColor: "transparent"
            strokeWidth: 0
            PathSvg { path: Icons.paths[root.name] || "" }
        }
    }
}
