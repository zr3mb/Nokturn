import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import Nokturn

// Przewijana strona: kolumna sekcji z marginesami i cienkim paskiem przewijania.
Flickable {
    id: root
    default property alias content: column.data
    property real padding: 34
    property alias spacing: column.spacing

    contentWidth: width
    contentHeight: column.implicitHeight + padding * 2
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    flickDeceleration: 3200
    maximumFlickVelocity: 4200

    ScrollBar.vertical: ThinScrollBar {}

    ColumnLayout {
        id: column
        x: root.padding
        y: root.padding
        width: root.width - root.padding * 2
        spacing: 34
    }
}
