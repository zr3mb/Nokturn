import QtQuick
import QtQuick.Layouts
import QtQuick.Shapes
import Nokturn

// Krok 7: podsumowanie wyborów.
Item {
    id: root

    // animowany znaczek: okrąg się domyka, potem "ptaszek"
    property real draw: 0
    readonly property real tick: Math.max(0, Math.min(1, (draw - 0.55) / 0.45))   // postęp rysowania "ptaszka"
    onVisibleChanged: if (visible) { draw = 0; drawAnim.restart() }
    NumberAnimation { id: drawAnim; target: root; property: "draw"; from: 0; to: 1; duration: 1100; easing.type: Easing.OutCubic }
    Component.onCompleted: drawAnim.start()

    readonly property var summary: [
        { icon: "menuOpen", label: "Panel boczny", value: Nav.sidebarCompact ? "kompaktowy" : "pełny" },
        { icon: "queue", label: "„Teraz odtwarzane”", value: Nav.queueOpen ? "widoczny" : "ukryty" },
        { icon: "sparkle", label: "Kolor akcentu", value: Theme.dynamicAccent ? "z okładki" : "stały" },
        { icon: "album", label: "Tło", value: Theme.ambient ? "żywe" : "spokojne" },
        { icon: "folder", label: "Foldery", value: Library.folders.length === 0 ? "brak" : Theme.plural(Library.folders.length, "folder", "foldery", "folderów") },
        { icon: "playCircle", label: "YouTube Music", value: YouTube.connected ? (YouTube.accountName || "połączono") : "bez konta" },
        { icon: "mix", label: "Przejścia", value: Player.crossfade > 0 ? Player.crossfade + " s · " + Player.mixStyleName(Player.crossfadeStyle).toLowerCase() : "wyłączone" },
        { icon: "chat", label: "Discord", value: Discord.enabled ? (Discord.connected ? "połączono" : "włączony") : "wyłączony" }
    ]

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width, 900)
        spacing: 20

        Shape {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 96
            Layout.preferredHeight: 96
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeColor: Theme.accent
                strokeWidth: 5
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                PathAngleArc { centerX: 48; centerY: 48; radiusX: 44; radiusY: 44; startAngle: -90; sweepAngle: 360 * Math.min(1, root.draw * 1.4) }
            }
            ShapePath {
                strokeColor: Theme.text
                strokeWidth: 6
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin
                startX: 30; startY: 50
                PathLine { x: 30 + 12 * Math.min(1, root.tick * 2); y: 50 + 12 * Math.min(1, root.tick * 2) }
                PathLine { x: 42 + 26 * Math.max(0, root.tick * 2 - 1); y: 62 - 26 * Math.max(0, root.tick * 2 - 1) }
            }
        }

        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: "Wszystko gotowe"
            color: Theme.text
            font.family: Theme.fontDisplay
            font.pixelSize: 44
            font.weight: Font.Bold
        }
        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: Library.localCount > 0 || YouTube.connected
                ? "Twoja muzyka czeka. Każde z tych ustawień zmienisz później w Ustawieniach."
                : "Możesz już szukać w YouTube Music. Foldery z muzyką dodasz w każdej chwili w zakładce Źródła."
            color: Theme.textDim
            font.pixelSize: 15
            wrapMode: Text.WordWrap
        }

        GridLayout {
            Layout.fillWidth: true
            Layout.topMargin: 12
            columns: 4
            columnSpacing: 12
            rowSpacing: 12
            Repeater {
                model: root.summary
                Rectangle {
                    id: chip
                    required property var modelData
                    required property int index
                    Layout.fillWidth: true
                    Layout.preferredHeight: 64
                    radius: 16
                    color: Theme.alpha(Theme.surface, 0.72)
                    border.width: 1
                    border.color: Qt.rgba(1, 1, 1, 0.06)
                    opacity: Math.max(0, Math.min(1, (root.draw * 8) - chip.index * 0.6))
                    RowLayout {
                        anchors { fill: parent; leftMargin: 14; rightMargin: 14 }
                        spacing: 12
                        Icon { name: chip.modelData.icon; size: 20; color: Theme.accent }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1
                            Text {
                                Layout.fillWidth: true
                                text: chip.modelData.label
                                color: Theme.textFaint
                                font.pixelSize: 11
                                elide: Text.ElideRight
                            }
                            Text {
                                Layout.fillWidth: true
                                text: chip.modelData.value
                                color: Theme.text
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }
        }
    }
}
