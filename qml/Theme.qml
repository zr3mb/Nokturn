pragma Singleton
import QtQuick
import Nokturn

// Palette: one of the themes below + an accent taken from the current track's cover art.
Item {
    id: theme

    // Every theme is dark: the covers, overlays and white-on-art text stay readable in all of them.
    // fixedAccent: the theme keeps its own accent instead of following the cover.
    readonly property var themes: ({
        nokturn: { name: "Nokturn", bg: "#101114", surface: "#1a1c20", surfaceHi: "#23262b",
                   text: "#e8e4dd", textDim: "#a19e98", textFaint: "#6e6d6a",
                   accent: "#9fb5a8", accent2: "#4a5d6e", onAccent: "#15161a", fixedAccent: false },
        midnight: { name: "Midnight", bg: "#000000", surface: "#0f1013", surfaceHi: "#191a1f",
                    text: "#ececf1", textDim: "#9c9ca6", textFaint: "#62626b",
                    accent: "#8fa8d8", accent2: "#2f3c63", onAccent: "#0b0d14", fixedAccent: false },
        sunset: { name: "Sunset", bg: "#15100f", surface: "#211917", surfaceHi: "#2c211e",
                  text: "#f1e6dc", textDim: "#b39f93", textFaint: "#7a6960",
                  accent: "#e0a27c", accent2: "#6b3b44", onAccent: "#1c120f", fixedAccent: false },
        cat: { name: "Cat", bg: "#110914", surface: "#1d1121", surfaceHi: "#2a172f",
                  text: "#fbeaf6", textDim: "#c9a3c2", textFaint: "#8a6a86",
                  accent: "#ff7ad6", accent2: "#8a3fd0", onAccent: "#2a0a23", fixedAccent: true }
    })
    readonly property var themeIds: ["nokturn", "midnight", "sunset", "cat"]

    property string themeId: Prefs.stringValue("ui/theme", "nokturn")
    onThemeIdChanged: Prefs.setValue("ui/theme", themeId)
    readonly property var preset: themes[themeId] || themes.nokturn
    readonly property bool cat: themeId === "cat"

    property color bg: preset.bg
    property color surface: preset.surface
    property color surfaceHi: preset.surfaceHi
    property color text: preset.text
    property color textDim: preset.textDim
    property color textFaint: preset.textFaint
    property color onAccent: preset.onAccent
    Behavior on bg { ColorAnimation { duration: 600 } }
    Behavior on surface { ColorAnimation { duration: 600 } }
    Behavior on surfaceHi { ColorAnimation { duration: 600 } }
    Behavior on text { ColorAnimation { duration: 600 } }
    Behavior on textDim { ColorAnimation { duration: 600 } }
    Behavior on textFaint { ColorAnimation { duration: 600 } }
    readonly property color success: "#9cc0a0"
    readonly property color danger: "#c98585"
    readonly property color warning: "#cdb388"

    // Selected pills (tabs, chips): light text color by default, the accent in the Cat theme.
    readonly property color selection: cat ? accent : text
    readonly property color onSelection: cat ? onAccent : "#15161a"
    readonly property color panelBorder: cat ? alpha(accent, 0.16) : Qt.rgba(1, 1, 1, 0.05)

    // Remembered between launches (Settings, first-run wizard).
    property bool dynamicAccent: Prefs.boolValue("ui/dynamicAccent", true)
    property bool ambient: Prefs.boolValue("ui/ambient", true)
    onDynamicAccentChanged: Prefs.setValue("ui/dynamicAccent", dynamicAccent)
    onAmbientChanged: Prefs.setValue("ui/ambient", ambient)
    readonly property color staticAccent: preset.accent
    readonly property color staticAccent2: preset.accent2
    readonly property bool followsCover: dynamicAccent && !preset.fixedAccent

    property color accent: followsCover ? Player.colorA : staticAccent
    property color accent2: followsCover ? Player.colorB : staticAccent2
    Behavior on accent { ColorAnimation { duration: 900; easing.type: Easing.InOutCubic } }
    Behavior on accent2 { ColorAnimation { duration: 900; easing.type: Easing.InOutCubic } }

    // Picture behind the interface (any theme), dimmed so the text stays readable.
    property string wallpaper: Prefs.stringValue("ui/wallpaper", "")
    property real wallpaperDim: Prefs.realValue("ui/wallpaperDim", 0.6)
    onWallpaperChanged: Prefs.setValue("ui/wallpaper", wallpaper)
    onWallpaperDimChanged: Prefs.setValue("ui/wallpaperDim", wallpaperDim)

    // Windows: Segoe UI Variable; Linux/macOS: the system font (e.g. Cantarell, Noto Sans, Inter).
    readonly property bool windows: Qt.platform.os === "windows"
    readonly property string fontDisplay: windows ? "Segoe UI Variable Display" : Qt.application.font.family
    readonly property string fontText: windows ? "Segoe UI Variable Text" : Qt.application.font.family

    readonly property int fast: 150
    readonly property int normal: 260
    readonly property int slow: 520

    // Bumped by main.cpp after a GPU reset: images then load again (they keep no memory copy).
    property int imageEpoch: 0

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
