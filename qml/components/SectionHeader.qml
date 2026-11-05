import QtQuick
import QtQuick.Layouts
import Everplay

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

        Rectangle {
            id: stroke
            Layout.preferredWidth: 0
            Layout.preferredHeight: 4
            Layout.bottomMargin: 6
            radius: Theme.r(2)
            color: Theme.accent
            SequentialAnimation {
                running: true
                PauseAnimation { duration: Theme.ms(260) }
                NumberAnimation { target: stroke; property: "Layout.preferredWidth"; to: 28; duration: Theme.ms(700); easing.type: Easing.OutQuint }
            }
        }

        Text {
            Layout.fillWidth: true
            text: root.title
            color: Theme.text
            font.family: Theme.fontDisplay
            font.pixelSize: 25
            font.weight: Font.Bold
            font.letterSpacing: -0.6
        }
        Text {
            visible: root.subtitle !== ""
            text: root.subtitle
            color: Theme.textDim
            font.pixelSize: 13
        }
    }

    Rectangle {
        visible: root.action !== ""
        Layout.alignment: Qt.AlignVCenter
        implicitWidth: actionText.implicitWidth + 26
        implicitHeight: 32
        radius: 16
        color: actionMouse.containsMouse ? Theme.fillHover : Theme.fill
        border.width: 1
        border.color: Theme.hairline
        Behavior on color { ColorAnimation { duration: Theme.fast } }

        Text {
            id: actionText
            anchors.centerIn: parent
            text: root.action
            color: actionMouse.containsMouse ? Theme.text : Theme.textDim
            font.pixelSize: 12
            font.weight: Font.Bold
            Behavior on color { ColorAnimation { duration: Theme.fast } }
        }

        MouseArea {
            id: actionMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.actionClicked()
        }
    }
}
