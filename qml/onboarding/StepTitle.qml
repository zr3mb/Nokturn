import QtQuick
import QtQuick.Layouts
import Nokturn

// Wizard step heading.
ColumnLayout {
    id: root
    property string title: ""
    property string subtitle: ""
    Layout.fillWidth: true
    spacing: 6

    Text {
        Layout.fillWidth: true
        text: root.title
        color: Theme.text
        font.family: Theme.fontDisplay
        font.pixelSize: 34
        font.weight: Font.Bold
        wrapMode: Text.WordWrap
    }
    Text {
        Layout.fillWidth: true
        visible: root.subtitle !== ""
        text: root.subtitle
        color: Theme.textDim
        font.pixelSize: 15
        wrapMode: Text.WordWrap
    }
}
