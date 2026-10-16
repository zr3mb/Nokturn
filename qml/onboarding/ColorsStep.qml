import QtQuick
import QtQuick.Layouts
import Nokturn

// Krok 3: kolor akcentu (z okładki / stały) i tło (żywe / spokojne).
Item {
    id: root

    ColumnLayout {
        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
        spacing: 26

        StepTitle {
            title: "Kolory i nastrój"
            subtitle: "Nokturn może przejmować barwy okładki odtwarzanego utworu i delikatnie pulsować światłem w rytm muzyki."
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 28

            ColumnLayout {
                Layout.fillWidth: true
                Layout.preferredWidth: 100
                spacing: 12
                GroupLabel { text: "Kolor akcentu" }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14
                    OptionCard {
                        title: "Z okładki"
                        description: "Przyciski, suwaki i światło płynnie zmieniają barwę razem z muzyką."
                        selected: Theme.dynamicAccent
                        onClicked: Theme.dynamicAccent = true
                        LayoutPreview {
                            anchors.fill: parent
                            compact: Nav.sidebarCompact
                            showQueue: Nav.queueOpen
                            dynamicAccent: true
                            ambient: Theme.ambient
                        }
                    }
                    OptionCard {
                        title: "Stały"
                        description: "Jeden, spokojny kolor szałwii — niezależnie od utworu."
                        selected: !Theme.dynamicAccent
                        onClicked: Theme.dynamicAccent = false
                        LayoutPreview {
                            anchors.fill: parent
                            compact: Nav.sidebarCompact
                            showQueue: Nav.queueOpen
                            dynamicAccent: false
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
                GroupLabel { text: "Tło" }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14
                    OptionCard {
                        title: "Żywe"
                        description: "Miękka poświata za interfejsem, pulsująca w rytm muzyki."
                        selected: Theme.ambient
                        onClicked: Theme.ambient = true
                        LayoutPreview {
                            anchors.fill: parent
                            compact: Nav.sidebarCompact
                            showQueue: Nav.queueOpen
                            dynamicAccent: Theme.dynamicAccent
                            ambient: true
                        }
                    }
                    OptionCard {
                        title: "Spokojne"
                        description: "Jednolite, ciemne tło — mniej ruchu, mniej pracy dla karty graficznej."
                        selected: !Theme.ambient
                        onClicked: Theme.ambient = false
                        LayoutPreview {
                            anchors.fill: parent
                            compact: Nav.sidebarCompact
                            showQueue: Nav.queueOpen
                            dynamicAccent: Theme.dynamicAccent
                            ambient: false
                        }
                    }
                }
            }
        }
    }
}
