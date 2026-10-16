import QtQuick
import QtQuick.Layouts
import Nokturn

// Korektor parametryczny: wykres z punktami pasm, fader wzmocnienia wstępnego z miernikiem
// i limiterem, edytor zaznaczonego pasma oraz presety.
Item {
    id: page
    property var args: ({})
    property int selected: 3

    readonly property var bandColors: ["#c98585", "#cdb388", "#a3bcab", "#8fb7b3", "#93a8c3", "#ab9fc3", "#cda2a6"]
    readonly property var typeNames: ["Low cut", "Niska półka", "Dzwon", "Wysoka półka", "High cut", "Notch", "Pasmowy"]
    readonly property var band: Equalizer.bands[selected]
    readonly property bool isCut: band.type === Equalizer.LowCut || band.type === Equalizer.HighCut

    function fmtFreq(f) {
        if (f >= 1000) return (f / 1000).toFixed(f >= 10000 ? 1 : 2).replace(/\.?0+$/, "") + " kHz"
        return Math.round(f) + " Hz"
    }
    function fmtDb(d) { return (d > 0 ? "+" : "") + d.toFixed(1) + " dB" }

    component Param: ColumnLayout {
        id: param
        property string label: ""
        property string valueText: ""
        property real value: 0
        property bool active: true
        property var bubble: null
        signal edited(real v)

        Layout.fillWidth: true
        Layout.preferredWidth: 100
        spacing: 8
        opacity: active ? 1 : 0.35

        RowLayout {
            Layout.fillWidth: true
            Text {
                Layout.fillWidth: true
                text: param.label
                color: Theme.textDim
                font.pixelSize: 12
                font.weight: Font.DemiBold
                font.letterSpacing: 0.6
            }
            Text {
                text: param.valueText
                color: Theme.text
                font.pixelSize: 13
                font.weight: Font.DemiBold
                font.features: { "tnum": 1 }
            }
        }
        ProgressSlider {
            Layout.fillWidth: true
            enabled: param.active
            value: param.value
            activeColor: page.bandColors[page.selected]
            labelFor: param.bubble
            onDragging: v => param.edited(v)
            onMoved: v => param.edited(v)
        }
    }

    PageFlick {
        anchors.fill: parent
        spacing: 26

        // ---- nagłówek -------------------------------------------------------------
        Reveal {
            RowLayout {
                width: parent.width
                spacing: 16

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Text {
                        Layout.fillWidth: true
                        text: "Korektor"
                        color: Theme.text
                        font.family: Theme.fontDisplay
                        font.pixelSize: 38
                        font.weight: Font.Bold
                    }
                    Text {
                        Layout.fillWidth: true
                        text: "Przeciągaj punkty · kółko myszy zmienia szerokość · dwuklik zeruje · prawy przycisk wyłącza pasmo"
                        color: Theme.textDim
                        font.pixelSize: 14
                        elide: Text.ElideRight
                    }
                }
                Text {
                    text: Equalizer.enabled ? "Włączony" : "Wyłączony"
                    color: Equalizer.enabled ? Theme.text : Theme.textDim
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                }
                Toggle {
                    checked: Equalizer.enabled
                    onToggled: c => Equalizer.enabled = c
                }
            }
        }

        // ---- presety ------------------------------------------------------------------
        Reveal {
            delay: 50
            Flow {
                width: parent.width
                spacing: 8

                Repeater {
                    model: Equalizer.presets
                    Rectangle {
                        id: chip
                        required property var modelData
                        readonly property bool current: Equalizer.preset === modelData
                        width: chipText.implicitWidth + 30
                        height: 34
                        radius: 17
                        color: current ? Theme.text : Theme.alpha(Theme.text, chipMouse.containsMouse ? 0.11 : 0.06)
                        Behavior on color { ColorAnimation { duration: Theme.normal } }
                        Text {
                            id: chipText
                            anchors.centerIn: parent
                            text: chip.modelData
                            color: chip.current ? "#15161a" : Theme.textDim
                            font.pixelSize: 13
                            font.weight: chip.current ? Font.DemiBold : Font.Medium
                        }
                        MouseArea {
                            id: chipMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: Equalizer.applyPreset(chip.modelData)
                        }
                    }
                }
                Text {
                    visible: Equalizer.preset === "Własne"
                    height: 34
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: 6
                    text: "Własne ustawienia"
                    color: Theme.accent
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                }
            }
        }

        // ---- wykres + master ------------------------------------------------------------
        Reveal {
            delay: 100
            RowLayout {
                width: parent.width
                spacing: 16

                EqGraph {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 380
                    colors: page.bandColors
                    selected: page.selected
                    onSelectRequested: i => page.selected = i
                }

                // Master: wzmocnienie wstępne, miernik wyjścia, limiter
                Rectangle {
                    Layout.preferredWidth: 150
                    Layout.preferredHeight: 380
                    radius: 20
                    color: Qt.rgba(0, 0, 0, 0.22)
                    border.width: 1
                    border.color: Qt.rgba(1, 1, 1, 0.06)

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 10

                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: "MASTER"
                            color: Theme.textFaint
                            font.pixelSize: 11
                            font.weight: Font.Bold
                            font.letterSpacing: 2
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            spacing: 12

                            Fader {
                                Layout.fillHeight: true
                                Layout.preferredWidth: 46
                                from: -Equalizer.maxPreamp
                                to: Equalizer.maxPreamp
                                value: Equalizer.preamp
                                onMoved: v => Equalizer.preamp = v
                            }

                            // Miernik: poziom po korekcji (przed limiterem), skala -48 … +12 dBFS
                            Item {
                                id: meter
                                Layout.fillHeight: true
                                Layout.preferredWidth: 12
                                readonly property real lo: -48
                                readonly property real hi: 12
                                function yOf(db) { return (1 - (Math.max(lo, Math.min(hi, db)) - lo) / (hi - lo)) * height }

                                Rectangle {
                                    anchors.fill: parent
                                    radius: 4
                                    color: Qt.rgba(1, 1, 1, 0.06)
                                }
                                Rectangle {
                                    anchors.bottom: parent.bottom
                                    width: parent.width
                                    height: parent.height - meter.yOf(Equalizer.peak)
                                    radius: 4
                                    gradient: Gradient {
                                        GradientStop { position: 0.0; color: Theme.danger }
                                        GradientStop { position: 0.2; color: Theme.warning }
                                        GradientStop { position: 0.45; color: Theme.success }
                                        GradientStop { position: 1.0; color: Theme.alpha(Theme.success, 0.5) }
                                    }
                                }
                                // 0 dBFS
                                Rectangle {
                                    y: meter.yOf(0)
                                    x: -3
                                    width: parent.width + 6
                                    height: 1
                                    color: Qt.rgba(1, 1, 1, 0.5)
                                }
                            }

                            // Redukcja przez limiter (rośnie w dół)
                            Item {
                                Layout.fillHeight: true
                                Layout.preferredWidth: 6
                                visible: Equalizer.limiter
                                Rectangle {
                                    anchors.fill: parent
                                    radius: 3
                                    color: Qt.rgba(1, 1, 1, 0.04)
                                }
                                Rectangle {
                                    width: parent.width
                                    radius: 3
                                    height: Math.min(1, Equalizer.reduction / 24) * parent.height
                                    color: Theme.warning
                                }
                            }
                        }

                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: page.fmtDb(Equalizer.preamp)
                            color: Theme.text
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            font.features: { "tnum": 1 }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 24
                            radius: 12
                            color: Equalizer.clipping ? Theme.alpha(Theme.danger, 0.85)
                                 : Equalizer.limiter && Equalizer.reduction > 0.5 ? Theme.alpha(Theme.warning, 0.25)
                                 : Qt.rgba(1, 1, 1, 0.05)
                            Behavior on color { ColorAnimation { duration: 120 } }
                            Text {
                                anchors.centerIn: parent
                                text: Equalizer.clipping ? "PRZESTER"
                                    : Equalizer.limiter && Equalizer.reduction > 0.5 ? "LIMIT −" + Equalizer.reduction.toFixed(1) + " dB"
                                    : "OK"
                                color: Equalizer.clipping ? "#ffffff" : Theme.textDim
                                font.pixelSize: 10
                                font.weight: Font.Bold
                                font.letterSpacing: 0.8
                            }
                        }

                        RowLayout {
                            Layout.alignment: Qt.AlignHCenter
                            spacing: 8
                            Text {
                                text: "Limiter"
                                color: Theme.textDim
                                font.pixelSize: 12
                            }
                            Toggle {
                                checked: Equalizer.limiter
                                onToggled: c => Equalizer.limiter = c
                            }
                        }
                    }
                }
            }
        }

        // ---- edytor pasma ------------------------------------------------------------------
        Reveal {
            delay: 150
            Rectangle {
                width: parent.width
                implicitHeight: editor.implicitHeight + 44
                radius: 20
                color: Qt.rgba(0, 0, 0, 0.18)
                border.width: 1
                border.color: Theme.alpha(page.bandColors[page.selected], 0.25)
                Behavior on border.color { ColorAnimation { duration: Theme.normal } }

                ColumnLayout {
                    id: editor
                    x: 22
                    y: 22
                    width: parent.width - 44
                    spacing: 20

                    // Wybór pasma
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Repeater {
                            model: 7
                            Rectangle {
                                id: bandChip
                                required property int index
                                readonly property bool sel: index === page.selected
                                readonly property var info: Equalizer.bands[index]
                                Layout.preferredWidth: 44
                                Layout.preferredHeight: 34
                                radius: 12
                                color: sel ? page.bandColors[index] : Theme.alpha(page.bandColors[index], bandMouse.containsMouse ? 0.22 : 0.12)
                                opacity: info.enabled ? 1 : 0.45
                                Behavior on color { ColorAnimation { duration: Theme.fast } }
                                Text {
                                    anchors.centerIn: parent
                                    text: bandChip.index + 1
                                    color: bandChip.sel ? "#15161a" : page.bandColors[bandChip.index]
                                    font.pixelSize: 14
                                    font.weight: Font.Bold
                                }
                                MouseArea {
                                    id: bandMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: page.selected = bandChip.index
                                }
                            }
                        }

                        Item { Layout.fillWidth: true }

                        Text {
                            text: page.band.enabled ? "Pasmo aktywne" : "Pasmo wyłączone"
                            color: Theme.textDim
                            font.pixelSize: 13
                        }
                        Toggle {
                            checked: page.band.enabled
                            onToggled: c => Equalizer.setBandEnabled(page.selected, c)
                        }
                        PillButton {
                            kind: "ghost"
                            icon: "refresh"
                            text: "Resetuj"
                            onClicked: Equalizer.resetBand(page.selected)
                        }
                    }

                    // Typ filtra
                    Flow {
                        Layout.fillWidth: true
                        spacing: 8
                        Repeater {
                            model: page.typeNames
                            Rectangle {
                                id: typeChip
                                required property var modelData
                                required property int index
                                readonly property bool current: page.band.type === index
                                width: typeText.implicitWidth + 28
                                height: 32
                                radius: 16
                                color: current ? Theme.alpha(page.bandColors[page.selected], 0.9)
                                               : Theme.alpha(Theme.text, typeMouse.containsMouse ? 0.1 : 0.05)
                                Behavior on color { ColorAnimation { duration: Theme.fast } }
                                Text {
                                    id: typeText
                                    anchors.centerIn: parent
                                    text: typeChip.modelData
                                    color: typeChip.current ? "#15161a" : Theme.textDim
                                    font.pixelSize: 12
                                    font.weight: typeChip.current ? Font.DemiBold : Font.Medium
                                }
                                MouseArea {
                                    id: typeMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: Equalizer.setBandType(page.selected, typeChip.index)
                                }
                            }
                        }
                    }

                    // Parametry
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 28

                        Param {
                            label: "CZĘSTOTLIWOŚĆ"
                            valueText: page.fmtFreq(page.band.freq)
                            value: Math.log(page.band.freq / 20) / Math.log(1000)
                            bubble: function(v) { return page.fmtFreq(20 * Math.pow(1000, v)) }
                            onEdited: v => Equalizer.setBandFreq(page.selected, 20 * Math.pow(1000, v))
                        }
                        Param {
                            label: "WZMOCNIENIE"
                            active: page.band.usesGain
                            valueText: page.band.usesGain ? page.fmtDb(page.band.gain) : "—"
                            value: (page.band.gain + Equalizer.maxGain) / (2 * Equalizer.maxGain)
                            bubble: function(v) { return page.fmtDb((v * 2 - 1) * Equalizer.maxGain) }
                            onEdited: v => Equalizer.setBandGain(page.selected, Math.round((v * 2 - 1) * Equalizer.maxGain * 10) / 10)
                        }
                        Param {
                            label: page.isCut ? "REZONANS (Q)" : "SZEROKOŚĆ (Q)"
                            valueText: page.band.q.toFixed(2)
                            value: Math.log(page.band.q / 0.1) / Math.log(180)
                            bubble: function(v) { return "Q " + (0.1 * Math.pow(180, v)).toFixed(2) }
                            onEdited: v => Equalizer.setBandQ(page.selected, 0.1 * Math.pow(180, v))
                        }
                    }

                    // Nachylenie dla filtrów odcinających
                    RowLayout {
                        visible: page.isCut
                        spacing: 12
                        Text {
                            text: "NACHYLENIE"
                            color: Theme.textDim
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                            font.letterSpacing: 0.6
                        }
                        SegmentedTabs {
                            model: ["12 dB/okt.", "24 dB/okt.", "48 dB/okt."]
                            currentIndex: page.band.slope >= 4 ? 2 : page.band.slope >= 2 ? 1 : 0
                            onActivated: i => Equalizer.setBandSlope(page.selected, [1, 2, 4][i])
                        }
                    }
                }
            }
        }
    }
}
