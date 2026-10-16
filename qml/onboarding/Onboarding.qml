import QtQuick
import QtQuick.Layouts
import Nokturn

// Kreator pierwszego uruchomienia: powitanie → układ → kolory → muzyka (foldery, YouTube Music)
// → odtwarzanie (miks, teksty) → integracje (Discord) → podsumowanie. Każdy wybór działa od razu
// i jest zapamiętywany; kreator można pominąć i uruchomić ponownie z Ustawień.
Item {
    id: root
    readonly property bool open: Nav.onboarding

    visible: opacity > 0.01
    opacity: open ? 1 : 0
    Behavior on opacity { NumberAnimation { duration: 520; easing.type: Easing.OutCubic } }

    readonly property var steps: [
        { key: "welcome", label: "Powitanie" },
        { key: "layout", label: "Układ" },
        { key: "colors", label: "Kolory" },
        { key: "music", label: "Muzyka" },
        { key: "playback", label: "Odtwarzanie" },
        { key: "integrations", label: "Integracje" },
        { key: "done", label: "Gotowe" }
    ]
    readonly property int step: Nav.onboardingStep
    readonly property bool last: step === steps.length - 1

    function go(i) {
        Nav.onboardingStep = Math.max(0, Math.min(steps.length - 1, i))
    }
    function next() {
        if (last) Nav.finishOnboarding()
        else go(step + 1)
    }

    Keys.onReturnPressed: next()
    Keys.onEnterPressed: next()
    focus: open

    Rectangle {
        anchors.fill: parent
        color: Theme.bg
    }
    AmbientBackground {
        anchors.fill: parent
        visible: Theme.ambient
        boost: 1.6
    }

    // Blokuje kliknięcia do aplikacji pod spodem
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.AllButtons
        onWheel: wheel => wheel.accepted = true
    }

    // ---- górny pasek: przeciąganie okna, logo, postęp ----
    Item {
        id: header
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 76

        DragHandler {
            target: null
            onActiveChanged: if (active) root.Window.window.startSystemMove()
        }

        Row {
            x: 28
            anchors.verticalCenter: parent.verticalCenter
            spacing: 12
            LogoMark {}
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "Nokturn"
                color: Theme.text
                font.family: Theme.fontDisplay
                font.pixelSize: 20
                font.weight: Font.Bold
            }
        }

        // postęp: segmenty z nazwami kroków (można klikać w odwiedzone)
        Row {
            anchors.centerIn: parent
            spacing: 6
            opacity: root.step > 0 ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: 400 } }
            Repeater {
                model: root.steps.length - 2   // bez powitania i podsumowania
                Item {
                    id: seg
                    required property int index
                    readonly property int stepIndex: index + 1
                    readonly property bool done: root.step > stepIndex
                    readonly property bool current: root.step === stepIndex
                    width: 104
                    height: 34
                    Rectangle {
                        anchors { left: parent.left; right: parent.right; top: parent.top }
                        height: 4
                        radius: 2
                        color: Theme.alpha(Theme.text, 0.1)
                        Rectangle {
                            width: seg.done || seg.current ? parent.width : 0
                            height: parent.height
                            radius: 2
                            color: seg.current ? Theme.accent : Theme.alpha(Theme.accent, 0.6)
                            Behavior on width { NumberAnimation { duration: 520; easing.type: Easing.OutQuint } }
                        }
                    }
                    Text {
                        anchors { left: parent.left; bottom: parent.bottom }
                        text: root.steps[seg.stepIndex].label
                        color: seg.current ? Theme.text : seg.done ? Theme.textDim : Theme.textFaint
                        font.pixelSize: 12
                        font.weight: seg.current ? Font.DemiBold : Font.Normal
                    }
                    MouseArea {
                        anchors.fill: parent
                        enabled: seg.stepIndex <= root.step + 1
                        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onClicked: root.go(seg.stepIndex)
                    }
                }
            }
        }

        WindowControls {
            anchors { right: parent.right; rightMargin: 10; verticalCenter: parent.verticalCenter }
        }
    }

    // ---- kroki ----
    Item {
        id: stage
        anchors { left: parent.left; right: parent.right; top: header.bottom; bottom: footer.top }
        clip: true

        Repeater {
            model: [welcomeStep, layoutStep, colorsStep, musicStep, playbackStep, integrationsStep, doneStep]
            Loader {
                id: page
                required property var modelData
                required property int index
                readonly property int offset: index - root.step
                width: Math.min(1120, stage.width - 80)
                height: stage.height
                anchors.horizontalCenter: parent.horizontalCenter
                sourceComponent: modelData
                // Wcześniej odwiedzone i następny krok zostają w pamięci (płynne przejście w obie strony).
                active: Math.abs(offset) <= 1 || item !== null

                opacity: offset === 0 ? 1 : 0
                enabled: offset === 0
                visible: opacity > 0.01
                transform: Translate { x: page.slide }
                property real slide: page.offset * 90
                Behavior on opacity { NumberAnimation { duration: 380; easing.type: Easing.OutCubic } }
                Behavior on slide { NumberAnimation { duration: 620; easing.type: Easing.OutQuint } }
            }
        }
    }

    // ---- dolny pasek ----
    Item {
        id: footer
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 92

        Rectangle {
            anchors { left: parent.left; right: parent.right; top: parent.top; leftMargin: 40; rightMargin: 40 }
            height: 1
            color: Qt.rgba(1, 1, 1, 0.06)
        }

        RowLayout {
            anchors { fill: parent; leftMargin: 40; rightMargin: 40 }
            spacing: 12

            PillButton {
                visible: root.step > 0
                kind: "ghost"
                icon: "chevronLeft"
                text: "Wstecz"
                onClicked: root.go(root.step - 1)
            }
            Item { Layout.fillWidth: true }
            Text {
                visible: !root.last
                text: "Pomiń konfigurację"
                color: skipMouse.containsMouse ? Theme.text : Theme.textDim
                font.pixelSize: 13
                font.underline: skipMouse.containsMouse
                MouseArea {
                    id: skipMouse
                    anchors.fill: parent
                    anchors.margins: -8
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: Nav.finishOnboarding()
                }
            }
            Item { width: 8; height: 1 }
            PillButton {
                kind: "primary"
                icon: root.last ? "play" : "chevronRight"
                text: root.step === 0 ? "Zaczynamy" : root.last ? "Zacznij słuchać" : "Dalej"
                implicitHeight: 46
                fontSize: 15
                onClicked: root.next()
            }
        }
    }

    Component { id: welcomeStep; WelcomeStep {} }
    Component { id: layoutStep; LayoutStep {} }
    Component { id: colorsStep; ColorsStep {} }
    Component { id: musicStep; MusicStep {} }
    Component { id: playbackStep; PlaybackStep {} }
    Component { id: integrationsStep; IntegrationsStep {} }
    Component { id: doneStep; DoneStep {} }
}
