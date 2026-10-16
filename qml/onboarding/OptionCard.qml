import QtQuick
import QtQuick.Layouts
import Nokturn

// Karta wyboru w kreatorze: podgląd (np. LayoutPreview) + tytuł + opis, zaznaczenie akcentem.
Item {
    id: root
    property bool selected: false
    property string title: ""
    property string description: ""
    default property alias preview: holder.data
    signal clicked

    Layout.fillWidth: true
    implicitWidth: 240
    implicitHeight: column.implicitHeight + 24

    scale: mouse.pressed ? 0.985 : mouse.containsMouse ? 1.015 : 1
    Behavior on scale { NumberAnimation { duration: 260; easing.type: Easing.OutCubic } }

    Rectangle {
        anchors.fill: parent
        radius: 18
        color: root.selected ? Theme.alpha(Theme.accent, 0.1) : Theme.alpha(Theme.text, mouse.containsMouse ? 0.06 : 0.035)
        border.width: root.selected ? 2 : 1
        border.color: root.selected ? Theme.accent : Theme.alpha(Theme.text, 0.08)
        Behavior on color { ColorAnimation { duration: Theme.normal } }
        Behavior on border.color { ColorAnimation { duration: Theme.normal } }
    }

    ColumnLayout {
        id: column
        x: 12
        y: 12
        width: root.width - 24
        spacing: 10

        Item {
            id: holder
            Layout.fillWidth: true
            Layout.preferredHeight: width * 0.62
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 4
            spacing: 10

            // znacznik wyboru
            Rectangle {
                width: 20; height: 20
                radius: 10
                color: root.selected ? Theme.accent : "transparent"
                border.width: root.selected ? 0 : 2
                border.color: Theme.alpha(Theme.text, 0.3)
                Behavior on color { ColorAnimation { duration: Theme.normal } }
                Icon {
                    anchors.centerIn: parent
                    name: "check"
                    size: 14
                    color: "#15161a"
                    scale: root.selected ? 1 : 0
                    Behavior on scale { NumberAnimation { duration: 360; easing.type: Easing.OutBack } }
                }
            }
            Text {
                Layout.fillWidth: true
                text: root.title
                color: Theme.text
                font.pixelSize: 15
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
        }
        Text {
            Layout.fillWidth: true
            Layout.leftMargin: 4
            Layout.bottomMargin: 2
            visible: root.description !== ""
            text: root.description
            color: Theme.textDim
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
