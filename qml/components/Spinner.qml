import QtQuick
import QtQuick.Shapes
import Nokturn

Item {
    id: root
    property real size: 18
    property color color: Theme.text
    property real thickness: Math.max(2, size / 9)

    implicitWidth: size
    implicitHeight: size

    Shape {
        id: arc
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: root.color
            strokeWidth: root.thickness
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            PathAngleArc {
                centerX: root.size / 2; centerY: root.size / 2
                radiusX: (root.size - root.thickness) / 2; radiusY: (root.size - root.thickness) / 2
                startAngle: 0; sweepAngle: 270
            }
        }

        RotationAnimator on rotation {
            running: root.visible
            from: 0; to: 360
            duration: 900
            loops: Animation.Infinite
        }
    }
}
