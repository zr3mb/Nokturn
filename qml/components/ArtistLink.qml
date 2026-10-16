import QtQuick
import Nokturn

// Artist name that opens the artist profile on click (underlined on hover).
Text {
    id: root
    property string artistId: ""       // YouTube Music channel; empty = look the artist up by name
    property bool linkEnabled: true

    font.underline: linkEnabled && mouse.containsMouse
    color: linkEnabled && mouse.containsMouse ? Theme.text : Theme.textDim

    MouseArea {
        id: mouse
        // Only the painted text is clickable, not the empty rest of the row.
        width: Math.min(root.contentWidth, root.width)
        height: root.height
        enabled: root.linkEnabled && root.text !== ""
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: Nav.openArtist(root.text, root.artistId)
    }
}
