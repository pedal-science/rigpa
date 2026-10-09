import QtQuick
import Rigpa.Core

// One widget, full height: the engine's connection, state, health, DSP load and xruns, then the
// input and output meters (off in settings keeps the screen still).
Rectangle {
    id: info
    color: Theme.panel

    readonly property var engine: App.engine
    readonly property bool connected: engine.connection === EngineSession.Connected

    function healthColor(h) { return h === "ok" ? Theme.good : h === "warn" ? Theme.warn : h === "error" ? Theme.bad : Theme.faint }

    Rectangle { width: 1; height: parent.height; color: Theme.line }

    Column {
        x: 24; y: 20
        width: parent.width - 48
        spacing: 10

        Text { text: qsTr("Engine"); color: Theme.dim; font.pixelSize: Theme.fontSmall }
        Row {
            spacing: 10
            width: parent.width
            Rectangle {
                width: 12; height: 12; radius: 6
                y: 6
                color: info.connected ? info.healthColor(info.engine.health)
                     : info.engine.connection === EngineSession.Connecting ? Theme.warn : Theme.bad
            }
            Text {
                width: parent.width - 22
                text: info.connected ? (info.engine.state || qsTr("connected")) : info.engine.connectionText
                color: Theme.text
                font.pixelSize: Theme.fontNormal
                wrapMode: Text.Wrap
                maximumLineCount: 3
                elide: Text.ElideRight
            }
        }

        Grid {
            visible: info.connected
            columns: 2
            columnSpacing: 16
            rowSpacing: 6
            Text { text: qsTr("Health"); color: Theme.dim; font.pixelSize: Theme.fontSmall }
            Text { text: info.engine.health; color: info.healthColor(info.engine.health); font.pixelSize: Theme.fontSmall }
            Text { text: qsTr("DSP"); color: Theme.dim; font.pixelSize: Theme.fontSmall }
            Text {
                text: info.engine.dspPercent < 0 ? "—" : Math.round(info.engine.dspPercent) + " %"
                color: info.engine.dspPercent >= 90 ? Theme.bad : info.engine.dspPercent >= 70 ? Theme.warn : Theme.text
                font.pixelSize: Theme.fontSmall
            }
            Text { text: qsTr("Xruns"); color: Theme.dim; font.pixelSize: Theme.fontSmall }
            Text { text: info.engine.xruns; color: info.engine.xruns > 0 ? Theme.warn : Theme.text; font.pixelSize: Theme.fontSmall }
            Text { text: qsTr("Audio"); color: Theme.dim; font.pixelSize: Theme.fontSmall }
            Text {
                text: info.engine.sampleRate > 0 ? (info.engine.sampleRate / 1000) + " kHz · " + info.engine.periodSize : "—"
                color: Theme.text
                font.pixelSize: Theme.fontSmall
            }
        }

        Text {
            visible: info.connected && info.engine.issue !== ""
            width: parent.width
            text: info.engine.issue
            color: Theme.warn
            font.pixelSize: Theme.fontSmall
            wrapMode: Text.Wrap
            maximumLineCount: 3
            elide: Text.ElideRight
        }
    }

    // The meters: -60 to 0 dBFS, inputs then outputs.
    Item {
        id: meters
        x: 24
        width: parent.width - 48
        height: 150
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 20

        Text {
            visible: !App.settings.liveMeters
            anchors.centerIn: parent
            text: qsTr("Meters off")
            color: Theme.faint
            font.pixelSize: Theme.fontSmall
        }

        Row {
            visible: App.settings.liveMeters
            anchors.bottom: parent.bottom
            spacing: 28
            Repeater {
                model: [
                    { label: qsTr("In"), peaks: info.engine.inputPeaks },
                    { label: qsTr("Out"), peaks: info.engine.outputPeaks }
                ]
                delegate: Column {
                    id: group
                    required property var modelData
                    spacing: 6
                    Row {
                        spacing: 6
                        Repeater {
                            model: Math.max(2, group.modelData.peaks.length)
                            delegate: Rectangle {
                                required property int index
                                readonly property real db: index < group.modelData.peaks.length ? group.modelData.peaks[index] : -60
                                width: 14
                                height: 120
                                radius: 3
                                color: Theme.raised
                                Rectangle {
                                    anchors.bottom: parent.bottom
                                    width: parent.width
                                    radius: 3
                                    height: parent.height * Math.max(0, (parent.db + 60) / 60)
                                    color: parent.db > -3 ? Theme.bad : parent.db > -12 ? Theme.warn : Theme.good
                                }
                            }
                        }
                    }
                    Text { text: group.modelData.label; color: Theme.dim; font.pixelSize: Theme.fontSmall }
                }
            }
        }
    }
}
