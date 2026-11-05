import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import Everplay

// Plugins: what's installed (on/off, settings, remove) and the catalog on the Everplay server.
// Installing anything asks first: a plugin is code that runs with the app's rights.
Item {
    id: page
    property var args: ({})
    property int tab: args && args.tab !== undefined ? args.tab : 0
    Component.onCompleted: if (tab === 1) Plugins.refreshCatalog()

    readonly property var kindNames: ({
        wallpaper: qsTr("Wallpaper"), overlay: qsTr("Overlay"), main: qsTr("Extension"), theme: qsTr("Theme")
    })

    // What waits for the "install anyway" in the warning: a file, or a catalog id.
    property var pending: null
    function ask(what) {
        pending = what
        warning.open()
    }

    onTabChanged: if (tab === 1 && Plugins.catalogState !== "ready") Plugins.refreshCatalog()

    component Chip: Rectangle {
        property string label: ""
        implicitWidth: chipText.implicitWidth + 16
        implicitHeight: 22
        radius: 11
        color: Theme.alpha(Theme.accent, 0.14)
        Text {
            id: chipText
            anchors.centerIn: parent
            text: parent.label
            color: Theme.accent
            font.pixelSize: 11
            font.weight: Font.DemiBold
        }
    }

    component Card: Rectangle {
        Layout.fillWidth: true
        radius: Theme.r(18)
        color: Theme.alpha(Theme.text, 0.04)
        border.width: 1
        border.color: Theme.alpha(Theme.text, 0.06)
    }

    component Thumb: Rectangle {
        property url picture
        property string icon: "grid"
        implicitWidth: 64
        implicitHeight: 64
        radius: Theme.r(14)
        gradient: Gradient {
            GradientStop { position: 0; color: Theme.alpha(Theme.accent, 0.5) }
            GradientStop { position: 1; color: Theme.alpha(Theme.accent2, 0.5) }
        }
        clip: true
        Icon {
            anchors.centerIn: parent
            visible: thumbImage.status !== Image.Ready
            name: parent.icon
            size: 28
            color: "#f4efe8"
        }
        Image {
            id: thumbImage
            anchors.fill: parent
            source: parent.picture
            sourceSize: Qt.size(128, 128)
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
        }
    }

    PageFlick {
        anchors.fill: parent

        Reveal {
            RowLayout {
                width: parent.width
                spacing: 16
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Text {
                        text: qsTr("Plugins")
                        color: Theme.text
                        font.family: Theme.fontDisplay
                        font.pixelSize: 38
                        font.weight: Font.Bold
                    }
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Animated wallpapers, themes and extras made by the community. Make your own — the guide shows how.")
                        color: Theme.textDim
                        font.pixelSize: 14
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }

        Reveal {
            delay: 40
            Flow {
                width: parent.width
                spacing: 10
                PillButton {
                    icon: "add"
                    text: qsTr("Install from file")
                    onClicked: fileDialog.open()
                }
                PillButton {
                    kind: "secondary"
                    icon: "folder"
                    text: qsTr("Plugins folder")
                    onClicked: Qt.openUrlExternally(Plugins.folder)
                }
                PillButton {
                    kind: "secondary"
                    icon: "edit"
                    text: qsTr("How to write a plugin")
                    onClicked: Qt.openUrlExternally(Plugins.docsUrl)
                }
                PillButton {
                    kind: "ghost"
                    icon: "refresh"
                    text: qsTr("Reload")
                    onClicked: {
                        Plugins.reload()
                        Nav.toast(qsTr("Plugins reloaded"), "refresh")
                    }
                }
            }
        }

        Reveal {
            delay: 70
            Item {
                width: parent.width
                implicitHeight: tabs.implicitHeight
                Rectangle {
                    anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                    height: 1
                    color: Qt.rgba(1, 1, 1, 0.06)
                }
                SegmentedTabs {
                    id: tabs
                    style: "underline"
                    model: [qsTr("Installed", "tab") + (Plugins.installed.length ? "  " + Plugins.installed.length : ""), qsTr("Catalog")]
                    currentIndex: page.tab
                    onActivated: i => page.tab = i
                }
            }
        }

        // ---- installed ----
        ColumnLayout {
            visible: page.tab === 0
            Layout.fillWidth: true
            spacing: 12

            Card {
                visible: Plugins.installed.length === 0
                implicitHeight: emptyCol.implicitHeight + 48
                ColumnLayout {
                    id: emptyCol
                    anchors.centerIn: parent
                    width: parent.width - 48
                    spacing: 8
                    Icon { Layout.alignment: Qt.AlignHCenter; name: "grid"; size: 40; color: Theme.textFaint }
                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        text: qsTr("No plugins yet")
                        color: Theme.text
                        font.pixelSize: 17
                        font.weight: Font.DemiBold
                    }
                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        text: qsTr("Browse the catalog, or install a plugin from a .zip file.")
                        color: Theme.textDim
                        font.pixelSize: 13
                    }
                    PillButton {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.topMargin: 6
                        kind: "secondary"
                        icon: "search"
                        text: qsTr("Browse the catalog")
                        onClicked: page.tab = 1
                    }
                }
            }

            Repeater {
                model: Plugins.installed

                Card {
                    id: card
                    required property var modelData
                    property bool confirmRemove: false
                    implicitHeight: cardRow.implicitHeight + 36
                    opacity: modelData.enabled ? 1 : 0.7

                    RowLayout {
                        id: cardRow
                        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: 18 }
                        spacing: 16

                        Thumb {
                            Layout.alignment: Qt.AlignTop
                            picture: card.modelData.preview
                            icon: card.modelData.kinds.indexOf("theme") >= 0 && card.modelData.kinds.length === 1 ? "image" : "grid"
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            RowLayout {
                                spacing: 8
                                Text {
                                    text: card.modelData.name
                                    color: Theme.text
                                    font.pixelSize: 16
                                    font.weight: Font.DemiBold
                                }
                                Text {
                                    visible: card.modelData.version !== ""
                                    text: card.modelData.version
                                    color: Theme.textFaint
                                    font.pixelSize: 12
                                }
                            }
                            Text {
                                visible: card.modelData.author !== ""
                                text: qsTr("by %1").arg(card.modelData.author)
                                color: Theme.textDim
                                font.pixelSize: 12
                            }
                            Text {
                                Layout.fillWidth: true
                                visible: text !== ""
                                text: card.modelData.description
                                color: Theme.textDim
                                font.pixelSize: 13
                                wrapMode: Text.WordWrap
                            }
                            Flow {
                                Layout.fillWidth: true
                                Layout.topMargin: 4
                                spacing: 6
                                Repeater {
                                    model: card.modelData.kinds
                                    Chip { required property string modelData; label: page.kindNames[modelData] || modelData }
                                }
                            }
                            Text {
                                Layout.fillWidth: true
                                visible: card.modelData.themes.length > 0 && card.modelData.enabled
                                text: qsTr("Themes: %1 — choose in Settings → Appearance").arg(card.modelData.themes.join(", "))
                                color: Theme.textFaint
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                            }
                            Text {
                                Layout.fillWidth: true
                                visible: text !== ""
                                text: card.modelData.error || Plugins.errors[card.modelData.id] || ""
                                color: Theme.danger
                                font.pixelSize: 12
                                font.family: "Consolas"
                                wrapMode: Text.WrapAnywhere
                                maximumLineCount: 6
                                elide: Text.ElideRight
                            }
                        }

                        ColumnLayout {
                            Layout.alignment: Qt.AlignTop
                            spacing: 8
                            Toggle {
                                Layout.alignment: Qt.AlignRight
                                checked: card.modelData.enabled
                                onToggled: checked => Plugins.setEnabled(card.modelData.id, checked)
                            }
                            RowLayout {
                                spacing: 2
                                IconButton {
                                    visible: card.modelData.settings.toString() !== "" && card.modelData.enabled
                                    icon: "tune"
                                    iconSize: 18
                                    showDot: false
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Settings")
                                    onClicked: settingsPopup.show(card.modelData)
                                }
                                IconButton {
                                    visible: card.modelData.homepage !== ""
                                    icon: "link"
                                    iconSize: 18
                                    showDot: false
                                    onClicked: Qt.openUrlExternally(card.modelData.homepage)
                                }
                                IconButton {
                                    icon: card.confirmRemove ? "check" : "close"
                                    iconSize: 18
                                    showDot: false
                                    color: card.confirmRemove ? Theme.danger : Theme.textDim
                                    ToolTip.visible: hovered
                                    ToolTip.text: card.confirmRemove ? qsTr("Click again to remove") : qsTr("Remove")
                                    onClicked: card.confirmRemove ? Plugins.remove(card.modelData.id) : card.confirmRemove = true
                                }
                            }
                        }
                    }
                }
            }
        }

        // ---- catalog ----
        ColumnLayout {
            visible: page.tab === 1
            Layout.fillWidth: true
            spacing: 12

            RowLayout {
                visible: Plugins.catalogState === "loading"
                spacing: 10
                Spinner { size: 16; color: Theme.textDim }
                Text { text: qsTr("Loading the catalog…"); color: Theme.textDim; font.pixelSize: 13 }
            }

            Card {
                visible: Plugins.catalogState === "error"
                implicitHeight: 76
                RowLayout {
                    anchors { fill: parent; leftMargin: 20; rightMargin: 14 }
                    spacing: 12
                    Icon { name: "close"; size: 20; color: Theme.danger }
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("The catalog couldn't be loaded. Check the connection and try again.")
                        color: Theme.textDim
                        font.pixelSize: 13
                        wrapMode: Text.WordWrap
                    }
                    PillButton {
                        kind: "secondary"
                        icon: "refresh"
                        text: qsTr("Try again")
                        onClicked: Plugins.refreshCatalog()
                    }
                }
            }

            Text {
                visible: Plugins.catalogState === "ready" && Plugins.catalog.length === 0
                text: qsTr("The catalog is empty for now.")
                color: Theme.textDim
                font.pixelSize: 13
            }

            Repeater {
                model: Plugins.catalog

                Card {
                    id: entry
                    required property var modelData
                    implicitHeight: entryRow.implicitHeight + 36

                    RowLayout {
                        id: entryRow
                        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: 18 }
                        spacing: 16

                        Thumb { Layout.alignment: Qt.AlignTop; picture: entry.modelData.preview }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            RowLayout {
                                spacing: 8
                                Text {
                                    text: entry.modelData.name
                                    color: Theme.text
                                    font.pixelSize: 16
                                    font.weight: Font.DemiBold
                                }
                                Text {
                                    text: entry.modelData.version
                                    color: Theme.textFaint
                                    font.pixelSize: 12
                                }
                            }
                            Text {
                                visible: entry.modelData.author !== ""
                                text: qsTr("by %1").arg(entry.modelData.author)
                                color: Theme.textDim
                                font.pixelSize: 12
                            }
                            Text {
                                Layout.fillWidth: true
                                visible: text !== ""
                                text: entry.modelData.description
                                color: Theme.textDim
                                font.pixelSize: 13
                                wrapMode: Text.WordWrap
                            }
                            Flow {
                                Layout.fillWidth: true
                                Layout.topMargin: 4
                                spacing: 6
                                Repeater {
                                    model: entry.modelData.kinds || []
                                    Chip { required property string modelData; label: page.kindNames[modelData] || modelData }
                                }
                            }
                        }

                        PillButton {
                            Layout.alignment: Qt.AlignTop
                            kind: entry.modelData.installed && !entry.modelData.update ? "ghost" : "primary"
                            icon: entry.modelData.update ? "refresh" : entry.modelData.installed ? "check" : "add"
                            text: entry.modelData.update ? qsTr("Update")
                                  : entry.modelData.installed ? qsTr("Installed", "catalog") : qsTr("Install", "catalog")
                            busy: Plugins.busyId === entry.modelData.id
                            enabled: (!entry.modelData.installed || entry.modelData.update) && Plugins.busyId === ""
                            onClicked: page.ask({ catalog: entry.modelData.id, name: entry.modelData.name })
                        }
                    }
                }
            }
        }
    }

    FileDialog {
        id: fileDialog
        title: qsTr("Choose a plugin")
        nameFilters: [qsTr("Everplay plugin (*.zip plugin.json)")]
        onAccepted: page.ask({ file: selectedFile, name: "" })
    }

    // The warning before any install
    Popup {
        id: warning
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(460, page.width - 40)
        padding: 24
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        Overlay.modal: Rectangle { color: "#99000000" }
        background: Rectangle {
            radius: Theme.r(20)
            color: Theme.surfaceHi
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.07)
        }

        contentItem: ColumnLayout {
            spacing: 14
            Text {
                Layout.fillWidth: true
                text: page.pending && page.pending.name ? qsTr("Install “%1”?").arg(page.pending.name) : qsTr("Install this plugin?")
                color: Theme.text
                font.family: Theme.fontDisplay
                font.pixelSize: 20
                font.weight: Font.Bold
                wrapMode: Text.WordWrap
            }
            Text {
                Layout.fillWidth: true
                text: qsTr("A plugin is code that runs inside Everplay with the same rights as the app: it can see what you play and use the internet. Install only plugins you trust.")
                color: Theme.textDim
                font.pixelSize: 13
                wrapMode: Text.WordWrap
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                Layout.topMargin: 6
                spacing: 10
                PillButton {
                    kind: "ghost"
                    text: qsTr("Cancel", "dialog")
                    onClicked: warning.close()
                }
                PillButton {
                    icon: "check"
                    text: qsTr("Install", "confirm")
                    onClicked: {
                        const p = page.pending
                        warning.close()
                        if (p && p.catalog)
                            Plugins.installFromCatalog(p.catalog)
                        else if (p && p.file)
                            Plugins.installFile(p.file)
                    }
                }
            }
        }
    }

    // A plugin's own settings screen (its "settings" QML), built when opened
    Popup {
        id: settingsPopup
        property var plugin: null
        property var instance: null
        function show(p) {
            plugin = p
            open()
            const c = Qt.createComponent(p.settings.toString() + "?r=" + Plugins.revision)
            instance = Plugins.instantiate(c, settingsBody, p.id)
            if (instance && instance.width !== undefined)
                instance.width = Qt.binding(() => settingsBody.width)
        }
        onClosed: { if (instance) instance.destroy(); instance = null }
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(520, page.width - 40)
        padding: 24
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        Overlay.modal: Rectangle { color: "#99000000" }
        background: Rectangle {
            radius: Theme.r(20)
            color: Theme.surfaceHi
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.07)
        }
        contentItem: ColumnLayout {
            spacing: 16
            RowLayout {
                Layout.fillWidth: true
                Text {
                    Layout.fillWidth: true
                    text: settingsPopup.plugin ? settingsPopup.plugin.name : ""
                    color: Theme.text
                    font.family: Theme.fontDisplay
                    font.pixelSize: 20
                    font.weight: Font.Bold
                    elide: Text.ElideRight
                }
                IconButton {
                    icon: "close"
                    iconSize: 18
                    showDot: false
                    onClicked: settingsPopup.close()
                }
            }
            Item {
                id: settingsBody
                Layout.fillWidth: true
                implicitHeight: settingsPopup.instance ? Math.max(settingsPopup.instance.height, settingsPopup.instance.implicitHeight) : 0
            }
        }
    }
}
