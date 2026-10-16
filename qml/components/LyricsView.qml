import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import Nokturn

// Tekst odtwarzanego utworu. Z czasami: bieżąca linia jest podświetlona i płynnie przewijana
// na środek, a kliknięcie linii przewija utwór. Bez czasów: zwykły tekst do czytania
// (opcjonalnie z przyciskiem dopasowania czasu przez rozpoznawanie mowy — alpha).
Item {
    id: root
    property real fontSize: 17
    property real lineSpacing: 10
    property bool large: false
    property int align: Text.AlignLeft

    readonly property int current: Lyrics.synced ? Lyrics.lineAt(Player.position + 0.15) : -1
    readonly property var currentLine: current >= 0 && current < Lyrics.lines.length ? Lyrics.lines[current] : null
    // Po ostatnim słowie linii (np. w przerwie instrumentalnej) podświetlenie gaśnie.
    readonly property bool currentActive: currentLine !== null && (currentLine.end < 0 || Player.position < currentLine.end)

    // Ręczne przewinięcie wstrzymuje śledzenie na kilka sekund.
    property bool follow: true
    Timer { id: resumeFollow; interval: 4000; onTriggered: { root.follow = true; root.recenter() } }

    function recenter() {
        if (!follow || current < 0 || list.moving)
            return
        const item = list.itemAtIndex(current)
        if (!item)
            return list.positionViewAtIndex(current, ListView.Center)
        const target = item.y + item.height / 2 - list.height * 0.42
        scroll.to = Math.max(list.originY, Math.min(target, list.contentHeight + list.originY - list.height))
        scroll.restart()
    }
    onCurrentChanged: recenter()
    onHeightChanged: recenter()
    Connections {
        target: Lyrics
        function onLyricsChanged() {
            root.follow = true
            list.positionViewAtBeginning()
        }
    }

    NumberAnimation { id: scroll; target: list; property: "contentY"; duration: 620; easing.type: Easing.OutCubic }

    // ---- stany bez tekstu ----------------------------------------------------
    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 24, 320)
        spacing: 12
        visible: Lyrics.state !== "ready"

        Spinner {
            Layout.alignment: Qt.AlignHCenter
            visible: Lyrics.state === "loading"
            size: 22
            color: Theme.textDim
        }
        Icon {
            Layout.alignment: Qt.AlignHCenter
            visible: Lyrics.state === "missing" || Lyrics.state === "error" || Lyrics.state === "none"
            name: Lyrics.state === "error" ? "close" : "lyrics"
            size: 26
            color: Theme.textFaint
        }
        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: Lyrics.state === "loading" ? "Szukam tekstu…"
                : Lyrics.state === "missing" ? "Brak tekstu dla tego utworu"
                : Lyrics.state === "error" ? "Nie udało się pobrać tekstu"
                : "Włącz utwór, żeby zobaczyć tekst"
            color: Theme.textDim
            font.pixelSize: 14
            wrapMode: Text.WordWrap
        }
        Text {
            Layout.fillWidth: true
            visible: Lyrics.state === "error" && Lyrics.error !== ""
            horizontalAlignment: Text.AlignHCenter
            text: Lyrics.error
            color: Theme.textFaint
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }
        PillButton {
            Layout.alignment: Qt.AlignHCenter
            visible: Lyrics.state === "error" || Lyrics.state === "missing"
            kind: "ghost"
            icon: "refresh"
            text: "Spróbuj ponownie"
            fontSize: 13
            implicitHeight: 34
            onClicked: Lyrics.retry()
        }
    }

    // ---- tekst ---------------------------------------------------------------
    ListView {
        id: list
        anchors { left: parent.left; right: parent.right; top: parent.top; bottom: footer.top; bottomMargin: 8 }
        visible: Lyrics.state === "ready"
        clip: true
        model: Lyrics.lines
        spacing: root.lineSpacing
        boundsBehavior: Flickable.StopAtBounds
        header: Item { height: Lyrics.synced ? list.height * 0.3 : 4 }
        footer: Item { height: Lyrics.synced ? list.height * 0.45 : 12 }
        ScrollBar.vertical: ThinScrollBar {}

        onMovementStarted: { root.follow = false; scroll.stop(); resumeFollow.stop() }
        onMovementEnded: resumeFollow.restart()

        delegate: Item {
            id: line
            required property var modelData
            required property int index
            readonly property bool blank: modelData.text === ""
            readonly property bool isCurrent: index === root.current
            readonly property bool past: Lyrics.synced && root.current >= 0 && index < root.current
            width: ListView.view.width
            height: blank ? root.fontSize * 0.6 : label.implicitHeight + 6

            Text {
                id: label
                width: parent.width
                y: 3
                visible: !line.blank
                text: line.modelData.text
                horizontalAlignment: root.align
                wrapMode: Text.WordWrap
                font.family: root.large ? Theme.fontDisplay : Theme.fontText
                font.pixelSize: root.fontSize
                font.weight: Lyrics.synced ? Font.Bold : Font.Medium
                lineHeight: 1.1
                color: !Lyrics.synced ? Theme.text
                     : line.isCurrent && root.currentActive ? Theme.text
                     : Theme.alpha(Theme.text, line.past || line.isCurrent ? 0.42 : 0.3)
                Behavior on color { ColorAnimation { duration: 260 } }
                scale: line.isCurrent && root.currentActive ? 1.0 : (Lyrics.synced ? 0.97 : 1.0)
                transformOrigin: root.align === Text.AlignHCenter ? Item.Center : Item.Left
                Behavior on scale { NumberAnimation { duration: 360; easing.type: Easing.OutCubic } }
            }

            // Delikatna poświata pod bieżącą linią w kolorze akcentu
            Rectangle {
                anchors { left: label.left; bottom: label.bottom; bottomMargin: -3 }
                visible: Lyrics.synced && !line.blank
                width: line.isCurrent && root.currentActive ? Math.min(label.paintedWidth, 48) : 0
                height: 3
                radius: 1.5
                color: Theme.accent
                x: root.align === Text.AlignHCenter ? (label.width - width) / 2 : 0
                Behavior on width { NumberAnimation { duration: 420; easing.type: Easing.OutCubic } }
            }

            HoverHandler { id: lineHover; enabled: Lyrics.synced && !line.blank; cursorShape: Qt.PointingHandCursor }
            TapHandler {
                enabled: Lyrics.synced && !line.blank && line.modelData.start >= 0
                onTapped: {
                    Player.seek(line.modelData.start)
                    root.follow = true
                }
            }
        }
    }

    // ---- stopka: źródło, dopasowanie (alpha) ----------------------------------
    ColumnLayout {
        id: footer
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        spacing: 8
        visible: Lyrics.state === "ready"
        height: visible ? implicitHeight : 0

        // Postęp rozpoznawania mowy
        ColumnLayout {
            Layout.fillWidth: true
            visible: Lyrics.aligning
            spacing: 6
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Spinner { size: 14; color: Theme.accent }
                Text {
                    Layout.fillWidth: true
                    text: (Lyrics.alignStage || "Dopasowywanie czasu") + " · " + Math.round(Lyrics.alignProgress * 100) + "%"
                    color: Theme.textDim
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }
                IconButton {
                    icon: "close"
                    iconSize: 14
                    implicitWidth: 26; implicitHeight: 26
                    showDot: false
                    onClicked: Lyrics.cancelAlign()
                }
            }
            Rectangle {
                Layout.fillWidth: true
                height: 3
                radius: 1.5
                color: Theme.alpha(Theme.text, 0.08)
                Rectangle {
                    width: parent.width * Lyrics.alignProgress
                    height: parent.height
                    radius: 1.5
                    color: Theme.accent
                    Behavior on width { NumberAnimation { duration: 300 } }
                }
            }
        }

        PillButton {
            visible: Lyrics.canAlign && !Lyrics.aligning
            kind: "secondary"
            icon: "sparkle"
            text: "Dopasuj czas (alpha)"
            fontSize: 12
            implicitHeight: 30
            onClicked: Lyrics.alignNow()
        }

        Text {
            Layout.fillWidth: true
            visible: text !== ""
            text: {
                const parts = []
                if (Lyrics.source !== "")
                    parts.push(Lyrics.source)
                if (Lyrics.origin === "stt")
                    parts.push("czas dopasowany automatycznie (alpha)")
                else if (!Lyrics.synced)
                    parts.push("bez synchronizacji")
                return parts.join(" · ")
            }
            color: Theme.textFaint
            font.pixelSize: 11
            wrapMode: Text.WordWrap
            horizontalAlignment: root.align
        }
    }

}
