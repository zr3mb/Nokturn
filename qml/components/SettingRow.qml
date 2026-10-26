import QtQuick
import QtQuick.Layouts
import Nokturn

// Settings row: title + description on the left, the control on the right
// (below them in the phone layout).
Item {
    id: root
    property string title: ""
    property string description: ""
    property string icon: ""
    default property alias control: slot.data

    Layout.fillWidth: true
    implicitHeight: Math.max(64, grid.implicitHeight + 28)

    Rectangle {
        anchors.fill: parent
        radius: 14
        color: Theme.alpha(Theme.text, hover.hovered ? 0.05 : 0.03)
        Behavior on color { ColorAnimation { duration: Theme.fast } }
    }
    HoverHandler { id: hover }

    GridLayout {
        id: grid
        anchors.fill: parent
        anchors.leftMargin: Nav.mobile ? 14 : 18
        anchors.rightMargin: Nav.mobile ? 14 : 18
        columns: Nav.mobile ? 2 : 3
        columnSpacing: Nav.mobile ? 12 : 16
        rowSpacing: 10

        Icon {
            visible: root.icon !== ""
            name: root.icon
            size: 20
            color: Theme.textDim
        }

        ColumnLayout {
            id: texts
            Layout.fillWidth: true
            spacing: 3

            Text {
                text: root.title
                color: Theme.text
                font.pixelSize: 15
                font.weight: Font.DemiBold
            }
            Text {
                Layout.fillWidth: true
                visible: root.description !== ""
                text: root.description
                color: Theme.textDim
                font.pixelSize: 13
                wrapMode: Text.WordWrap
            }
        }

        Item {
            id: slot
            Layout.columnSpan: Nav.mobile ? 2 : 1
            Layout.leftMargin: Nav.mobile && root.icon !== "" ? 32 : 0
            Layout.maximumWidth: Nav.mobile ? root.width - 28 : Number.POSITIVE_INFINITY
            visible: children.length > 0
            Layout.preferredWidth: childrenRect.width
            Layout.preferredHeight: childrenRect.height
        }
    }
}
