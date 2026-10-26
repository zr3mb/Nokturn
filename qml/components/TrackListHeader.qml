import QtQuick
import QtQuick.Layouts
import Nokturn

// Track table header — columns aligned with TrackRow.
Item {
    id: root
    property bool showAlbum: true

    Layout.fillWidth: true
    implicitHeight: Nav.mobile ? 0 : 38   // phone rows have no columns

    component Head: Text {
        color: Theme.textFaint
        font.pixelSize: 11
        font.weight: Font.DemiBold
        font.letterSpacing: 1.2
    }

    RowLayout {
        visible: !Nav.mobile
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 10
        spacing: 14

        Head { Layout.preferredWidth: 26; horizontalAlignment: Text.AlignHCenter; text: "#" }
        Item { Layout.preferredWidth: 42 }
        Head { Layout.fillWidth: true; text: qsTr("TITLE") }
        Head { visible: root.showAlbum; Layout.preferredWidth: root.width * 0.22; text: qsTr("ALBUM") }
        Head { Layout.preferredWidth: 132; text: qsTr("SOURCE") }
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
        visible: !Nav.mobile
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 1
        color: Qt.rgba(1, 1, 1, 0.06)
    }
}
