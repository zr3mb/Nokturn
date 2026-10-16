pragma Singleton
import QtQuick
import Nokturn

// Palette: warm, muted black + an accent taken from the current track's cover art.
Item {
    id: theme

    readonly property color bg: "#101114"
    readonly property color surface: "#1a1c20"
    readonly property color surfaceHi: "#23262b"
    readonly property color text: "#e8e4dd"
    readonly property color textDim: "#a19e98"
    readonly property color textFaint: "#6e6d6a"
    readonly property color success: "#9cc0a0"
    readonly property color danger: "#c98585"
    readonly property color warning: "#cdb388"

    // Remembered between launches (Settings, first-run wizard).
    property bool dynamicAccent: Prefs.boolValue("ui/dynamicAccent", true)
    property bool ambient: Prefs.boolValue("ui/ambient", true)
    onDynamicAccentChanged: Prefs.setValue("ui/dynamicAccent", dynamicAccent)
    onAmbientChanged: Prefs.setValue("ui/ambient", ambient)
    readonly property color staticAccent: "#9fb5a8"
    readonly property color staticAccent2: "#4a5d6e"

    property color accent: dynamicAccent ? Player.colorA : staticAccent
    property color accent2: dynamicAccent ? Player.colorB : staticAccent2
    Behavior on accent { ColorAnimation { duration: 900; easing.type: Easing.InOutCubic } }
    Behavior on accent2 { ColorAnimation { duration: 900; easing.type: Easing.InOutCubic } }

    // Windows: Segoe UI Variable; Linux/macOS: the system font (e.g. Cantarell, Noto Sans, Inter).
    readonly property bool windows: Qt.platform.os === "windows"
    readonly property string fontDisplay: windows ? "Segoe UI Variable Display" : Qt.application.font.family
    readonly property string fontText: windows ? "Segoe UI Variable Text" : Qt.application.font.family

    readonly property int fast: 150
    readonly property int normal: 260
    readonly property int slow: 520

    function alpha(c, a) {
        const col = Qt.lighter(c, 1.0)
        return Qt.rgba(col.r, col.g, col.b, a)
    }

    function formatTime(sec) {
        const s = Math.max(0, Math.floor(sec))
        const m = Math.floor(s / 60)
        const r = s % 60
        return m + ":" + (r < 10 ? "0" : "") + r
    }

    function formatTotal(sec) {
        const m = Math.round(sec / 60)
        if (m < 60) return qsTr("%1 min").arg(m)
        return qsTr("%1 h %2 min").arg(Math.floor(m / 60)).arg(m % 60)
    }

    function roundedRectPath(w, h, r) {
        if (w <= 0 || h <= 0) return ""
        r = Math.max(0, Math.min(r, w / 2, h / 2))
        return `M ${r} 0 H ${w - r} A ${r} ${r} 0 0 1 ${w} ${r} V ${h - r} A ${r} ${r} 0 0 1 ${w - r} ${h} `
             + `H ${r} A ${r} ${r} 0 0 1 0 ${h - r} V ${r} A ${r} ${r} 0 0 1 ${r} 0 Z`
    }

    function greeting() {
        const h = new Date().getHours()
        if (h < 5) return qsTr("Good night")
        if (h < 12) return qsTr("Good morning")
        if (h < 18) return qsTr("Good afternoon")
        return qsTr("Good evening")
    }
}
