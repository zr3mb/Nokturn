import QtQuick
import QtQuick.Layouts
import Nokturn

RowLayout {
    id: root
    property string title: ""
    property string subtitle: ""
    property string action: ""
    signal actionClicked

    width: parent ? parent.width : implicitWidth
    spacing: 12

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 2

        Text {
            Layout.fillWidth: true
            text: root.title
            color: Theme.text
            font.family: Theme.fontDisplay
            font.pixelSize: 22
            font.weight: Font.DemiBold
        }
        Text {
            visible: root.subtitle !== ""
            text: root.subtitle
            color: Theme.textDim
            font.pixelSize: 13
        }
    }

    Text {
        visible: root.action !== ""
        Layout.alignment: Qt.AlignBottom
        text: root.action
        color: actionMouse.containsMouse ? Theme.text : Theme.textDim
        font.pixelSize: 13
        font.weight: Font.DemiBold
        font.underline: actionMouse.containsMouse
        Behavior on color { ColorAnimation { duration: Theme.fast } }

        MouseArea {
            id: actionMouse
            anchors.fill: parent
            anchors.margins: -6
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.actionClicked()
        }
    }
}
