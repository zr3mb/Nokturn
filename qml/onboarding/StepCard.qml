import QtQuick
import QtQuick.Layouts
import Nokturn

// Karta z treścią kroku (źródła muzyki, odtwarzanie, integracje).
Rectangle {
    id: root
    property string icon: ""
    property string title: ""
    property string description: ""
    default property alias content: body.data

    Layout.fillWidth: true
    Layout.alignment: Qt.AlignTop
    implicitHeight: column.implicitHeight + 44
    radius: 20
    color: Theme.alpha(Theme.surface, 0.72)
    border.width: 1
    border.color: Qt.rgba(1, 1, 1, 0.06)

    ColumnLayout {
        id: column
        x: 22
        y: 22
        width: root.width - 44
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            visible: root.title !== ""
            spacing: 12
            Rectangle {
                visible: root.icon !== ""
                width: 38; height: 38
                radius: 12
                color: Theme.alpha(Theme.accent, 0.16)
                Icon { anchors.centerIn: parent; name: root.icon; size: 20; color: Theme.accent }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text {
                    Layout.fillWidth: true
                    text: root.title
                    color: Theme.text
                    font.pixelSize: 17
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
        }

        ColumnLayout {
            id: body
            Layout.fillWidth: true
            spacing: 12
        }
    }
}
