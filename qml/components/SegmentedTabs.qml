import QtQuick
import Nokturn

// Tabs with a sliding highlight: "pill" or "underline" style.
Item {
    id: root
    property var model: []
    property int currentIndex: 0
    property string style: "pill"
    readonly property bool pill: style === "pill"
    signal activated(int index)

    implicitHeight: pill ? 36 : 40
    implicitWidth: row.implicitWidth

    Rectangle {
        id: highlight
        readonly property Item target: rep.count > 0 ? rep.itemAt(root.currentIndex) : null
        visible: target !== null
        x: target ? target.x : 0
        width: target ? target.width : 0
        height: root.pill ? root.height : 2
        y: root.pill ? 0 : root.height - 2
        radius: root.pill ? height / 2 : 1
        color: root.pill ? Theme.selection : Theme.accent
        Behavior on x { NumberAnimation { duration: 420; easing.type: Easing.OutQuint } }
        Behavior on width { NumberAnimation { duration: 420; easing.type: Easing.OutQuint } }
    }

    Row {
        id: row
        height: root.height
        spacing: root.pill ? 8 : 26

        Repeater {
            id: rep
            model: root.model

            Item {
                id: tab
                required property int index
                required property var modelData
                readonly property bool selected: index === root.currentIndex
                width: label.implicitWidth + (root.pill ? 34 : 0)
                height: root.height

                Rectangle {
                    visible: root.pill
                    anchors.fill: parent
                    radius: height / 2
                    color: Theme.alpha(Theme.text, tabMouse.containsMouse ? 0.11 : 0.06)
                    opacity: tab.selected ? 0 : 1
                    Behavior on color { ColorAnimation { duration: Theme.fast } }
                }

                Text {
                    id: label
                    anchors.centerIn: parent
                    anchors.verticalCenterOffset: root.pill ? 0 : -2
                    text: tab.modelData
                    color: tab.selected ? (root.pill ? Theme.onSelection : Theme.text)
                                        : (tabMouse.containsMouse ? Theme.text : Theme.textDim)
                    font.pixelSize: root.pill ? 13 : 15
                    font.weight: tab.selected ? Font.DemiBold : Font.Medium
                    Behavior on color { ColorAnimation { duration: Theme.normal } }
                }

                MouseArea {
                    id: tabMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.currentIndex = tab.index
                        root.activated(tab.index)
                    }
                }
            }
        }
    }
}
