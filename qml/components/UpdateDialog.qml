import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import Nokturn

// “New version available” dialog (Updater): the change list from the GitHub release,
// download with progress and one-click install.
Item {
    id: root
    readonly property bool busy: Updater.state === "downloading" || Updater.state === "installing"
    readonly property bool shown: Updater.prompt

    visible: opacity > 0.01
    opacity: shown ? 1 : 0
    Behavior on opacity { NumberAnimation { duration: Theme.normal } }

    // Dimmed background — clicking outside closes it (unless something is downloading)
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.55)
        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            onClicked: if (!root.busy) Updater.later()
        }
    }

    Rectangle {
        id: card
        anchors.centerIn: parent
        width: 520
        height: content.implicitHeight + 56
        radius: 24
        color: Theme.surfaceHi
        border.width: 1
        border.color: Theme.alpha(Theme.text, 0.08)
        scale: root.shown ? 1 : 0.94
        Behavior on scale { NumberAnimation { duration: 420; easing.type: Easing.OutBack } }
        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowBlur: 1.0
            shadowOpacity: 0.6
            shadowVerticalOffset: 14
        }
        MouseArea { anchors.fill: parent }   // clicks on the card don't close the dialog

        ColumnLayout {
            id: content
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 28 }
            spacing: 18

            RowLayout {
                spacing: 16
                LogoMark {}
                ColumnLayout {
                    spacing: 3
                    Text {
                        text: qsTr("New version available")
                        color: Theme.text
                        font.family: Theme.fontDisplay
                        font.pixelSize: 22
                        font.weight: Font.Bold
                    }
                    Text {
                        text: qsTr("Nokturn %1  ·  you have %2").arg(Updater.latestVersion).arg(Updater.currentVersion)
                        color: Theme.textDim
                        font.pixelSize: 14
                    }
                }
            }

            // Change list (release notes in Markdown)
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(220, notes.implicitHeight + 28)
                visible: Updater.notes !== ""
                radius: 14
                color: Theme.alpha(Theme.text, 0.04)

                Flickable {
                    id: flick
                    anchors.fill: parent
                    anchors.margins: 14
                    clip: true
                    contentHeight: notes.implicitHeight
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ThinScrollBar {}

                    Text {
                        id: notes
                        width: flick.width - 8
                        text: Updater.notes
                        textFormat: Text.MarkdownText
                        wrapMode: Text.WordWrap
                        color: Theme.textDim
                        linkColor: Theme.accent
                        font.pixelSize: 14
                        onLinkActivated: link => Qt.openUrlExternally(link)
                    }
                }
            }

            // Download progress
            ColumnLayout {
                Layout.fillWidth: true
                visible: root.busy
                spacing: 8
                Rectangle {
                    Layout.fillWidth: true
                    height: 6
                    radius: 3
                    color: Theme.alpha(Theme.text, 0.1)
                    Rectangle {
                        width: parent.width * (Updater.state === "installing" ? 1 : Updater.progress)
                        height: parent.height
                        radius: 3
                        color: Theme.accent
                        Behavior on width { NumberAnimation { duration: 200 } }
                    }
                }
                Text {
                    text: Updater.state === "installing"
                          ? qsTr("Installing the update — Nokturn will restart in a moment…")
                          : qsTr("Downloading… %1%").arg(Math.round(Updater.progress * 100))
                    color: Theme.textDim
                    font.pixelSize: 13
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                visible: !root.busy

                PillButton {
                    text: qsTr("Skip this version")
                    kind: "ghost"
                    onClicked: Updater.skip()
                }
                Item { Layout.fillWidth: true }
                PillButton {
                    text: qsTr("Later")
                    kind: "secondary"
                    onClicked: Updater.later()
                }
                PillButton {
                    text: Updater.canInstall ? qsTr("Update now") : qsTr("Download")
                    icon: "refresh"
                    onClicked: Updater.install()
                }
            }
        }
    }
}
