import QtQuick

// Invisible edges of the frameless window that let you resize it.
Item {
    id: root
    property int grip: 6
    readonly property var win: Window.window

    component Edge: MouseArea {
        property int edges: 0
        enabled: root.enabled
        onPressed: root.win.startSystemResize(edges)
    }

    Edge { x: 0; y: root.grip; width: root.grip; height: root.height - 2 * root.grip; edges: Qt.LeftEdge; cursorShape: Qt.SizeHorCursor }
    Edge { x: root.width - root.grip; y: root.grip; width: root.grip; height: root.height - 2 * root.grip; edges: Qt.RightEdge; cursorShape: Qt.SizeHorCursor }
    Edge { x: root.grip; y: 0; width: root.width - 2 * root.grip; height: root.grip; edges: Qt.TopEdge; cursorShape: Qt.SizeVerCursor }
    Edge { x: root.grip; y: root.height - root.grip; width: root.width - 2 * root.grip; height: root.grip; edges: Qt.BottomEdge; cursorShape: Qt.SizeVerCursor }

    Edge { x: 0; y: 0; width: root.grip * 2; height: root.grip * 2; edges: Qt.LeftEdge | Qt.TopEdge; cursorShape: Qt.SizeFDiagCursor }
    Edge { x: root.width - root.grip * 2; y: 0; width: root.grip * 2; height: root.grip * 2; edges: Qt.RightEdge | Qt.TopEdge; cursorShape: Qt.SizeBDiagCursor }
    Edge { x: 0; y: root.height - root.grip * 2; width: root.grip * 2; height: root.grip * 2; edges: Qt.LeftEdge | Qt.BottomEdge; cursorShape: Qt.SizeBDiagCursor }
    Edge { x: root.width - root.grip * 2; y: root.height - root.grip * 2; width: root.grip * 2; height: root.grip * 2; edges: Qt.RightEdge | Qt.BottomEdge; cursorShape: Qt.SizeFDiagCursor }
}
