import QtQuick
import QtQuick.Shapes
import Nokturn

// Podgląd przejścia między utworami: obwiednia kończącego się utworu i wchodzącego następnego
// (długość nakładania = crossfade), z przesuwającą się głowicą odtwarzania.
Item {
    id: root
    property real seconds: 6          // 0 = bez przejścia
    property string style: "fade"
    property bool running: true
    implicitHeight: 110

    readonly property real span: 28   // sekund pokazanych na osi
    readonly property real overlap: Math.max(0.0001, seconds) / span * width
    readonly property real cut: width * 0.55          // koniec pierwszego utworu
    readonly property real startB: seconds > 0 ? cut - overlap : cut + 6

    property real head: 0
    NumberAnimation on head { from: 0; to: 1; duration: 7000; loops: Animation.Infinite; running: root.running && root.visible }

    Rectangle {
        anchors.fill: parent
        radius: 14
        color: Theme.alpha(Theme.text, 0.035)
    }

    // fala (pseudo) jako tło obu utworów
    Row {
        id: waves
        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: 12 }
        height: parent.height - 34
        spacing: 2
        Repeater {
            model: Math.floor((root.width - 24) / 5)
            Rectangle {
                required property int index
                readonly property real x0: index * 5 + 12
                readonly property real gA: x0 < root.startB ? 1 : x0 > root.cut ? 0 : Math.cos((x0 - root.startB) / Math.max(1, root.cut - root.startB) * Math.PI / 2)
                readonly property real gB: x0 < root.startB ? 0 : x0 > root.cut ? 1 : Math.sin((x0 - root.startB) / Math.max(1, root.cut - root.startB) * Math.PI / 2)
                readonly property real amp: 0.35 + 0.65 * Math.abs(Math.sin(index * 1.7) * Math.cos(index * 0.31))
                anchors.verticalCenter: parent.verticalCenter
                width: 3
                height: Math.max(2, waves.height * amp * Math.max(gA, gB))
                radius: 1.5
                color: gA >= gB ? Theme.alpha("#c9c3ba", 0.55) : Theme.alpha(Theme.accent, 0.85)
                opacity: x0 / root.width < root.head ? 1 : 0.5
            }
        }
    }

    // obwiednie głośności
    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: Theme.alpha("#efebe5", 0.7)
            strokeWidth: 2
            fillColor: "transparent"
            startX: 12; startY: 18
            PathLine { x: Math.max(12, root.startB); y: 18 }
            PathQuad { x: root.cut; y: root.height - 16; controlX: root.cut - (root.cut - root.startB) * 0.15; controlY: 18 }
        }
        ShapePath {
            strokeColor: Theme.accent
            strokeWidth: 2
            fillColor: "transparent"
            startX: root.startB; startY: root.height - 16
            PathQuad { x: root.cut; y: 18; controlX: root.startB + (root.cut - root.startB) * 0.15; controlY: 18 }
            PathLine { x: root.width - 12; y: 18 }
        }
    }

    // głowica odtwarzania
    Rectangle {
        x: 12 + (root.width - 24) * root.head
        y: 8
        width: 2
        height: root.height - 16
        radius: 1
        color: "#efebe5"
    }

    Text {
        anchors { left: parent.left; bottom: parent.bottom; leftMargin: 14; bottomMargin: 4 }
        text: "Utwór 1"
        color: Theme.textFaint
        font.pixelSize: 10
    }
    Text {
        anchors { right: parent.right; bottom: parent.bottom; rightMargin: 14; bottomMargin: 4 }
        text: root.seconds > 0 ? "Utwór 2 · nakładanie " + root.seconds + " s" : "Utwór 2 · bez nakładania"
        color: Theme.accent
        font.pixelSize: 10
    }
}
