import QtQuick
import Nokturn

// Pionowy suwak (fader) w decybelach: przeciąganie, kółko myszy, dwuklik = 0 dB.
Item {
    id: f
    property real value: 0
    property real from: -24
    property real to: 24
    property color color: Theme.accent
    signal moved(real value)

    implicitWidth: 46
    implicitHeight: 260

    readonly property real trackTop: 12
    readonly property real trackSpan: height - 24
    function yOf(v) { return trackTop + (1 - (v - from) / (to - from)) * trackSpan }
    function valueAt(y) {
        const v = from + (1 - Math.max(0, Math.min(1, (y - trackTop) / trackSpan))) * (to - from)
        return Math.round(v * 10) / 10
    }

    // Tor
    Rectangle {
        x: (f.width - width) / 2
        y: f.trackTop
        width: 6
        height: f.trackSpan
        radius: 3
        color: Qt.rgba(1, 1, 1, 0.08)
    }
    // Wypełnienie od 0 dB do wartości
    Rectangle {
        x: (f.width - width) / 2
        width: 6
        radius: 3
        y: Math.min(f.yOf(0), f.yOf(f.value))
        height: Math.abs(f.yOf(f.value) - f.yOf(0))
        color: f.value >= 0 ? f.color : Theme.alpha(f.color, 0.45)
    }
    // Znacznik 0 dB
    Rectangle {
        x: (f.width - width) / 2
        y: f.yOf(0)
        width: 22
        height: 1
        color: Qt.rgba(1, 1, 1, 0.3)
    }

    // Uchwyt
    Rectangle {
        id: handle
        width: f.width - 6
        height: 20
        radius: 7
        x: 3
        y: f.yOf(f.value) - height / 2
        color: mouse.pressed ? Qt.lighter(Theme.text, 1.05) : Theme.text
        scale: mouse.pressed ? 1.06 : 1
        Behavior on scale { NumberAnimation { duration: 160 } }
        Column {
            anchors.centerIn: parent
            spacing: 3
            Repeater {
                model: 3
                Rectangle { width: 16; height: 1; color: Qt.rgba(0, 0, 0, 0.35) }
            }
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        preventStealing: true
        cursorShape: Qt.SizeVerCursor
        onPressed: mouse => f.moved(f.valueAt(mouse.y))
        onPositionChanged: mouse => { if (pressed) f.moved(f.valueAt(mouse.y)) }
        onDoubleClicked: f.moved(0)
        onWheel: wheel => f.moved(Math.max(f.from, Math.min(f.to, f.value + wheel.angleDelta.y / 120 * 0.5)))
    }
}
