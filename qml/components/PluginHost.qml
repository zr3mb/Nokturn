import QtQuick
import Everplay

// One place in the window where plugins draw (docs/plugins.md): "wallpaper" behind the panels,
// "overlay" above everything (it never takes the mouse), "main" for logic without a look of its own.
// Each enabled plugin with a file for this slot gets its QML built here, with `plugin` (its
// PluginContext) set when the root declares that property. A file that fails shows on its card.
Item {
    id: root
    property string slot: "wallpaper"

    Repeater {
        model: {
            const r = Plugins.revision   // new files: build again
            return Plugins.installed.filter(p => p.enabled && p[root.slot] && p[root.slot].toString() !== "")
                                    .map(p => ({ id: p.id, url: p[root.slot].toString() + "?r=" + r }))
        }

        Item {
            id: host
            required property var modelData
            anchors.fill: parent
            property var component: null
            property var instance: null

            function build() {
                if (component.status === Component.Loading)
                    return
                instance = Plugins.instantiate(component, host, modelData.id)
                if (instance && instance.anchors !== undefined && root.slot !== "main")
                    instance.anchors.fill = host
            }

            Component.onCompleted: {
                component = Qt.createComponent(modelData.url, Component.Asynchronous)
                if (component.status === Component.Loading)
                    component.statusChanged.connect(build)
                else
                    build()
            }
            // the instance is a child of the host and goes with it
        }
    }
}
