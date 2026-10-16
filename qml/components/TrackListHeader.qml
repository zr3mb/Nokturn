import QtQuick
import QtQuick.Layouts
import Nokturn

// Nagłówek tabeli utworów — kolumny wyrównane z TrackRow.
Item {
    id: root
    property bool showAlbum: true

    Layout.fillWidth: true
    implicitHeight: 38

    component Head: Text {
        color: Theme.textFaint
        font.pixelSize: 11
        font.weight: Font.DemiBold
        font.letterSpacing: 1.2
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 10
        spacing: 14

        Head { Layout.preferredWidth: 26; horizontalAlignment: Text.AlignHCenter; text: "#" }
        Item { Layout.preferredWidth: 42 }
        Head { Layout.fillWidth: true; text: "TYTUŁ" }
        Head { visible: root.showAlbum; Layout.preferredWidth: root.width * 0.22; text: "ALBUM" }
        Head { Layout.preferredWidth: 132; text: "ŹRÓDŁO" }
        Item { Layout.preferredWidth: 38 }
        Item {
            Layout.preferredWidth: 52
            Layout.preferredHeight: 16
            Icon {
                anchors.right: parent.right
                name: "clock"
                size: 16
                color: Theme.textFaint
            }
        }
        Item { Layout.preferredWidth: 38 }
    }

    Rectangle {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 1
        color: Qt.rgba(1, 1, 1, 0.06)
    }
}
