import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Effects
import Nokturn

// In-game HUD lyrics overlay: displays lyrics over games while playing with click-through transparency
// and configurable background dimming.
Window {
    id: win

    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool | Qt.NoDropShadowWindowHint
           | (Theme.windows ? Qt.WindowDoesNotAcceptFocus : 0)
           | Qt.WindowTransparentForInput
    transientParent: null
    color: "transparent"
    title: "Nokturn Lyrics"
    x: GameOverlay.area.x
    y: GameOverlay.area.y
    width: Math.max(640, GameOverlay.area.width)
    height: Math.max(480, GameOverlay.area.height)

    property bool shown: false
    property real fade: shown ? 1 : 0
    Behavior on fade { NumberAnimation { duration: win.shown ? 260 : 180; easing.type: Easing.OutCubic } }
    visible: shown || fade > 0

    property bool prepared: false
    function open() {
        if (!prepared) {
            GameOverlay.prepare(win)
            prepared = true
        }
        shown = true
        GameOverlay.activateLyrics(win)
    }
    Component.onCompleted: if (GameOverlay.lyricsOpen) open()

    Connections {
        target: GameOverlay
        function onLyricsOpenChanged() {
            if (GameOverlay.lyricsOpen)
                win.open()
            else
                win.shown = false
        }
    }

    // ---- Background dimming over the game (configurable opacity) ----
    Rectangle {
        anchors.fill: parent
        color: "#000000"
        opacity: GameOverlay.lyricsDimming * win.fade
        Behavior on opacity { NumberAnimation { duration: 200 } }
    }

    // ---- HUD content container ----
    Item {
        id: hud
        anchors.fill: parent
        opacity: win.fade
        scale: 0.98 + 0.02 * win.fade
        Behavior on scale { NumberAnimation { duration: 260; easing.type: Easing.OutCubic } }

        readonly property real s: Math.max(0.7, Math.min(width / 1920, height / 1080))
        readonly property real lyricsTime: Player.position - Lyrics.offset
        readonly property int current: Lyrics.synced ? Lyrics.lineAt(lyricsTime + 0.15) : -1
        readonly property var currentLine: current >= 0 && current < Lyrics.lines.length ? Lyrics.lines[current] : null
        readonly property bool currentActive: currentLine !== null && (currentLine.end < 0 || lyricsTime < currentLine.end)

        function sungChars(line, time) {
            const words = line.words || []
            let n = 0
            for (let i = 0; i < words.length; ++i) {
                const w = words[i]
                if (time < w[0])
                    break
                n = time >= w[1] ? w[2] + w[3] : w[2] + Math.floor(w[3] * (time - w[0]) / Math.max(0.05, w[1] - w[0]))
            }
            return n
        }

        function escaped(text) {
            return text.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
        }

        function karaokeText(text, n) {
            return "<font color=\"" + String(Theme.accent) + "\">" + escaped(text.slice(0, n)) + "</font>" + escaped(text.slice(n))
        }

        // Parse lines into verse sections (split on empty lines, or chunks of 4)
        readonly property var sections: {
            const lines = Lyrics.lines || []
            if (lines.length === 0)
                return []
            const out = []
            let cur = []
            let startIdx = 0
            const hasBlanks = lines.some(l => (l.text || "").trim() === "")

            if (hasBlanks) {
                for (let i = 0; i < lines.length; ++i) {
                    const l = lines[i]
                    if ((l.text || "").trim() === "") {
                        if (cur.length > 0) {
                            out.push({ startIndex: startIdx, endIndex: i - 1, lines: cur })
                            cur = []
                        }
                        startIdx = i + 1
                    } else {
                        cur.push({ index: i, line: l })
                    }
                }
                if (cur.length > 0)
                    out.push({ startIndex: startIdx, endIndex: lines.length - 1, lines: cur })
            } else {
                for (let i = 0; i < lines.length; i += 4) {
                    const chunk = []
                    for (let j = i; j < Math.min(lines.length, i + 4); ++j)
                        chunk.push({ index: j, line: lines[j] })
                    out.push({ startIndex: i, endIndex: Math.min(lines.length - 1, i + 3), lines: chunk })
                }
            }
            return out
        }

        // Active section based on current line index
        readonly property var activeSection: {
            if (sections.length === 0)
                return null
            if (current < 0)
                return sections[0]
            for (let i = 0; i < sections.length; ++i) {
                const s = sections[i]
                if (current >= s.startIndex && current <= s.endIndex)
                    return s
            }
            return sections[sections.length - 1]
        }

        // Position HUD according to GameOverlay.lyricsPosition ("bottom", "center", "top")
        Item {
            id: hudBox
            width: Math.min(hud.width - 64 * hud.s, 960 * hud.s)
            height: contentLayout.implicitHeight + 40 * hud.s
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: GameOverlay.lyricsPosition === "top" ? parent.top : undefined
            anchors.topMargin: GameOverlay.lyricsPosition === "top" ? 56 * hud.s : 0
            anchors.bottom: GameOverlay.lyricsPosition === "bottom" ? parent.bottom : undefined
            anchors.bottomMargin: GameOverlay.lyricsPosition === "bottom" ? 56 * hud.s : 0
            anchors.verticalCenter: GameOverlay.lyricsPosition === "center" ? parent.verticalCenter : undefined

            // Backdrop panel with rounded corners and subtle blur/glow for high contrast over any game visual
            Rectangle {
                anchors.fill: parent
                radius: 18 * hud.s
                color: "#c0101216"
                border.width: 1
                border.color: Theme.alpha(Theme.text, 0.12)

                // Soft accent highlight line on top
                Rectangle {
                    anchors.top: parent.top
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: parent.width * 0.4
                    height: 2
                    radius: 1
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0; color: "transparent" }
                        GradientStop { position: 0.5; color: Theme.alpha(Theme.accent, 0.6) }
                        GradientStop { position: 1; color: "transparent" }
                    }
                }
            }

            ColumnLayout {
                id: contentLayout
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: 22 * hud.s
                spacing: 12 * hud.s

                // Mini header: Now playing pill
                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 8 * hud.s
                    visible: Player.hasTrack

                    Rectangle {
                        width: 8 * hud.s
                        height: 8 * hud.s
                        radius: 4 * hud.s
                        color: Player.playing ? Theme.accent : Theme.textDim
                    }

                    Text {
                        text: (Player.title ? Player.title : qsTr("Unknown track")) + (Player.artist ? " — " + Player.artist : "")
                        color: Theme.textDim
                        font.family: Theme.fontDisplay
                        font.pixelSize: 13 * hud.s
                        font.weight: Font.Medium
                        elide: Text.ElideRight
                        style: Text.Outline
                        styleColor: "#b0000000"
                    }
                }

                // ---- State: No track playing ----
                Text {
                    Layout.fillWidth: true
                    Layout.topMargin: 8 * hud.s
                    Layout.bottomMargin: 8 * hud.s
                    visible: !Player.hasTrack
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("Play music in Nokturn to see lyrics")
                    color: Theme.textDim
                    font.family: Theme.fontDisplay
                    font.pixelSize: 18 * hud.s
                    font.weight: Font.DemiBold
                    style: Text.Outline
                    styleColor: "#b0000000"
                }

                // ---- State: Lyrics loading or missing ----
                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: 6 * hud.s
                    Layout.bottomMargin: 6 * hud.s
                    visible: Player.hasTrack && Lyrics.state !== "ready"
                    spacing: 10 * hud.s

                    Spinner {
                        size: 18 * hud.s
                        color: Theme.accent
                        visible: Lyrics.state === "loading"
                    }
                    Text {
                        text: Lyrics.state === "loading" ? qsTr("Loading lyrics…")
                            : Lyrics.state === "missing" ? qsTr("No lyrics found for this track")
                            : qsTr("Lyrics unavailable")
                        color: Theme.textDim
                        font.family: Theme.fontDisplay
                        font.pixelSize: 17 * hud.s
                        font.weight: Font.DemiBold
                        style: Text.Outline
                        styleColor: "#b0000000"
                    }
                }

                // ==============================================================
                // MODE 1: KARAOKE STYLE
                // ==============================================================
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8 * hud.s
                    visible: Player.hasTrack && Lyrics.state === "ready" && GameOverlay.lyricsMode === "karaoke"

                    // Previous line (dimmed context)
                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        visible: hud.current > 0 && hud.current - 1 < Lyrics.lines.length && Lyrics.lines[hud.current - 1].text !== ""
                        text: visible ? Lyrics.lines[hud.current - 1].text : ""
                        color: Theme.alpha(Theme.text, 0.35)
                        font.family: Theme.fontText
                        font.pixelSize: 16 * hud.s
                        font.weight: Font.Normal
                        elide: Text.ElideRight
                        style: Text.Outline
                        styleColor: "#90000000"
                        Behavior on text { FadeTextAnimation {} }
                    }

                    // Main active singing line
                    Item {
                        Layout.fillWidth: true
                        implicitHeight: mainLineText.implicitHeight + 10 * hud.s

                        Text {
                            id: mainLineText
                            anchors.centerIn: parent
                            width: Math.min(parent.width, implicitWidth)
                            horizontalAlignment: Text.AlignHCenter
                            readonly property bool hasWords: (hud.currentLine && hud.currentLine.words && hud.currentLine.words.length > 0)
                            readonly property bool isKaraoke: Lyrics.karaoke && hud.currentActive && hasWords
                            text: {
                                if (!hud.currentLine)
                                    return Lyrics.synced ? "♪ ♪ ♪" : (Lyrics.lines[0] ? Lyrics.lines[0].text : "")
                                if (isKaraoke)
                                    return hud.karaokeText(hud.currentLine.text, hud.sungChars(hud.currentLine, hud.lyricsTime))
                                return hud.currentLine.text || "♪ ♪ ♪"
                            }
                            textFormat: isKaraoke ? Text.StyledText : Text.PlainText
                            color: hud.currentActive ? Theme.text : Theme.alpha(Theme.text, 0.7)
                            font.family: Theme.fontDisplay
                            font.pixelSize: 26 * hud.s
                            font.weight: Font.Bold
                            wrapMode: Text.WordWrap
                            style: Text.Outline
                            styleColor: "#d0000000"
                            scale: hud.currentActive ? 1.0 : 0.96
                            Behavior on scale { NumberAnimation { duration: 240; easing.type: Easing.OutCubic } }
                        }

                        // Glow indicator under current line
                        Rectangle {
                            anchors.bottom: parent.bottom
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: Math.min(mainLineText.width, 160 * hud.s)
                            height: 3 * hud.s
                            radius: 1.5 * hud.s
                            color: Theme.accent
                            opacity: hud.currentActive ? 0.9 : 0.25
                            Behavior on opacity { NumberAnimation { duration: 220 } }
                        }
                    }

                    // Upcoming next line (preview)
                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        visible: hud.current >= 0 && hud.current + 1 < Lyrics.lines.length && Lyrics.lines[hud.current + 1].text !== ""
                        text: visible ? Lyrics.lines[hud.current + 1].text : ""
                        color: Theme.alpha(Theme.text, 0.52)
                        font.family: Theme.fontText
                        font.pixelSize: 18 * hud.s
                        font.weight: Font.Medium
                        elide: Text.ElideRight
                        style: Text.Outline
                        styleColor: "#90000000"
                        Behavior on text { FadeTextAnimation {} }
                    }
                }

                // ==============================================================
                // MODE 2: SECTIONS (ZWROTKI / BLOKI)
                // ==============================================================
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6 * hud.s
                    visible: Player.hasTrack && Lyrics.state === "ready" && GameOverlay.lyricsMode === "sections"

                    Repeater {
                        model: hud.activeSection ? hud.activeSection.lines : []

                        Item {
                            required property var modelData
                            required property int index
                            Layout.fillWidth: true
                            implicitHeight: sectionLineText.implicitHeight + 6 * hud.s
                            readonly property bool isCurrent: modelData.index === hud.current

                            RowLayout {
                                anchors.centerIn: parent
                                width: Math.min(parent.width, implicitWidth)
                                spacing: 10 * hud.s

                                // Active line accent pill indicator
                                Rectangle {
                                    Layout.alignment: Qt.AlignVCenter
                                    width: 4 * hud.s
                                    height: 18 * hud.s
                                    radius: 2 * hud.s
                                    color: Theme.accent
                                    opacity: isCurrent ? 1 : 0
                                    scale: isCurrent ? 1 : 0.4
                                    Behavior on opacity { NumberAnimation { duration: 200 } }
                                    Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutBack } }
                                }

                                Text {
                                    id: sectionLineText
                                    text: modelData.line.text || ""
                                    horizontalAlignment: Text.AlignLeft
                                    color: isCurrent ? Theme.text : Theme.alpha(Theme.text, 0.55)
                                    font.family: isCurrent ? Theme.fontDisplay : Theme.fontText
                                    font.pixelSize: (isCurrent ? 22 : 18) * hud.s
                                    font.weight: isCurrent ? Font.Bold : Font.Normal
                                    wrapMode: Text.WordWrap
                                    style: Text.Outline
                                    styleColor: "#d0000000"
                                    Behavior on font.pixelSize { NumberAnimation { duration: 200 } }
                                    Behavior on color { ColorAnimation { duration: 200 } }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    component FadeTextAnimation: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 220; easing.type: Easing.OutCubic }
    }
}
