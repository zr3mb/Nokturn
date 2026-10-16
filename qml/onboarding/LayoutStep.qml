import QtQuick
import QtQuick.Layouts
import Nokturn

// Krok 2: układ okna — panel boczny i panel "Teraz odtwarzane", na żywych miniaturach.
Item {
    id: root

    ColumnLayout {
        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
        spacing: 26

        StepTitle {
            title: "Jak ułożyć okno?"
            subtitle: "Kliknij wariant, który Ci odpowiada — aplikacja zmieni się od razu. Miniatury pokazują, jak to wygląda w działaniu."
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 28

            ColumnLayout {
                Layout.fillWidth: true
                Layout.preferredWidth: 100
                spacing: 12
                GroupLabel { text: "Panel boczny" }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14
                    OptionCard {
                        title: "Pełny"
                        description: "Ikony z nazwami i Twoje playlisty zawsze pod ręką."
                        selected: !Nav.sidebarCompact
                        onClicked: Nav.sidebarCompact = false
                        LayoutPreview {
                            anchors.fill: parent
                            compact: false
                            showQueue: Nav.queueOpen
                            dynamicAccent: Theme.dynamicAccent
                            ambient: Theme.ambient
                        }
                    }
                    OptionCard {
                        title: "Kompaktowy"
                        description: "Same ikony — więcej miejsca na okładki i listy."
                        selected: Nav.sidebarCompact
                        onClicked: Nav.sidebarCompact = true
                        LayoutPreview {
                            anchors.fill: parent
                            compact: true
                            showQueue: Nav.queueOpen
                            dynamicAccent: Theme.dynamicAccent
                            ambient: Theme.ambient
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillHeight: true
                Layout.topMargin: 28
                width: 1
                color: Qt.rgba(1, 1, 1, 0.07)
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.preferredWidth: 100
                spacing: 12
                GroupLabel { text: "Panel „Teraz odtwarzane”" }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14
                    OptionCard {
                        title: "Widoczny"
                        description: "Okładka, kolejka i tekst utworu po prawej stronie."
                        selected: Nav.queueOpen
                        onClicked: Nav.queueOpen = true
                        LayoutPreview {
                            anchors.fill: parent
                            compact: Nav.sidebarCompact
                            showQueue: true
                            dynamicAccent: Theme.dynamicAccent
                            ambient: Theme.ambient
                        }
                    }
                    OptionCard {
                        title: "Ukryty"
                        description: "Otwierasz go przyciskiem na pasku odtwarzacza."
                        selected: !Nav.queueOpen
                        onClicked: Nav.queueOpen = false
                        LayoutPreview {
                            anchors.fill: parent
                            compact: Nav.sidebarCompact
                            showQueue: false
                            dynamicAccent: Theme.dynamicAccent
                            ambient: Theme.ambient
                        }
                    }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            text: "Wskazówka: tryb pełnoekranowy z dużą okładką i tekstem włączysz klawiszem F11."
            color: Theme.textFaint
            font.pixelSize: 13
        }
    }
}
