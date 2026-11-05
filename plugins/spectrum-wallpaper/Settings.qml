import QtQuick
import QtQuick.Layouts
import Nokturn

// This plugin's settings screen: Nokturn shows it in a window from the plugin's card (the tune button).
// It uses Nokturn's own controls (SegmentedTabs, Fader…) and keeps the values with plugin.setSetting.
ColumnLayout {
    property var plugin
    spacing: 14

    Text {
        text: Language.effective === "pl" ? "Styl" : "Style"
        color: Theme.textDim
        font.pixelSize: 13
    }
    SegmentedTabs {
        readonly property var styles: ["both", "glow", "bars"]
        model: [Language.effective === "pl" ? "Światło i spektrum" : "Light and spectrum",
                Language.effective === "pl" ? "Tylko światło" : "Light only",
                Language.effective === "pl" ? "Tylko spektrum" : "Spectrum only"]
        currentIndex: Math.max(0, styles.indexOf(plugin ? plugin.setting("style", "both") : "both"))
        onActivated: i => plugin.setSetting("style", styles[i])
    }

    Text {
        Layout.topMargin: 6
        text: (Language.effective === "pl" ? "Siła reakcji na muzykę: " : "How strongly it reacts: ")
              + Math.round((plugin && plugin.revision >= 0 ? plugin.setting("intensity", 0.7) : 0.7) * 100) + "%"
        color: Theme.textDim
        font.pixelSize: 13
    }
    ProgressSlider {
        Layout.fillWidth: true
        value: plugin ? plugin.setting("intensity", 0.7) : 0.7
        onMoved: v => plugin.setSetting("intensity", v)
        onDragging: v => plugin.setSetting("intensity", v)
    }
}
