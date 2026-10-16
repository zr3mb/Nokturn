import QtQuick
import QtQuick.Layouts
import Nokturn

// Settings row: title + description on the left, the control on the right.
Item {
    id: root
    property string title: ""
    property string description: ""
    property string icon: ""
    default property alias control: slot.data

    Layout.fillWidth: true
    implicitHeight: Math.max(64, texts.implicitHeight + 28)

    Rectangle {
        anchors.fill: parent
        radius: 14
        color: Theme.alpha(Theme.text, hover.hovered ? 0.05 : 0.03)
        Behavior on color { ColorAnimation { duration: Theme.fast } }
    }
    HoverHandler { id: hover }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 18
        anchors.rightMargin: 18
        spacing: 16

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
            Layout.preferredWidth: childrenRect.width
            Layout.preferredHeight: childrenRect.height
        }
    }
}
