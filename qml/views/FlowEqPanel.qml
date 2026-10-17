import QtQuick
import QtQuick.Shapes
import Rigpa
import Rigpa.Core

// flowEQ's own node editor (nc.eq.flow, ../namchain/docs/floweq.md). The generic editor would show
// its eight parameters as one row of knobs; the lite plugin's own layout is what this follows:
// X, Y, Z and Strength on top, In and Out with Extend and Auto Gain below, and the thing the
// plugin's visualiser draws — the response curve the *engine* computed — beside them.
//
// Everything is a rigpa control: rotary knobs (a drag, the hardware main rotary, and a double tap
// for the default), the engine's toggles, and a 0 dB grid. The curve is read from the engine with
// `rig.state` (FlowEqModel), never computed here: flowEQ's curve is the model's own output and a
// second implementation of it in the UI would be a second answer to what the EQ is doing.
Rectangle {
    id: panel
    readonly property var model: App.nodeEditor
    readonly property var flow: model.flowEq
    readonly property FocusRing ring: model.ring
    readonly property bool active: App.nav.overlay === Navigator.NodeEditor

    // Where each parameter sits, by its schema id: row-major reading order, which is also the
    // order of NodeEditorModel's controls and so of the ring (docs/spec.md).
    readonly property var cells: ({
        "x": [0, 0], "y": [1, 0], "z": [2, 0], "strength": [3, 0],
        "in_db": [0, 1], "out_db": [1, 1], "extend": [2, 1], "auto_gain": [3, 1]
    })
    readonly property real cellWidth: 194
    readonly property real cellHeight: 145
    readonly property real gridX: 704
    readonly property real gridY: 70

    color: Theme.background

    NodeTitleRow {
        x: 24
        y: 10
        width: panel.width - 48
        model: panel.model
        active: panel.active
    }

    // --- the response the engine is applying -------------------------------------------------
    Rectangle {
        id: curvePanel
        x: 24
        y: panel.gridY
        width: 660
        height: 290
        radius: Theme.radius
        color: Theme.panel
        border.width: 1
        border.color: Theme.line

        readonly property real plotX: 72
        readonly property real plotY: 24
        readonly property real plotW: panel.flow.curveWidth
        readonly property real plotH: panel.flow.curveHeight

        Text {
            x: curvePanel.plotX
            y: 4
            text: qsTr("RESPONSE")
            color: Theme.faint
            font.pixelSize: 13
            font.weight: Font.DemiBold
        }
        Text { // which decoder is running, for the curious and for a bug report
            anchors.right: parent.right
            anchors.rightMargin: 20
            y: 4
            text: panel.flow.available ? qsTr("model %1").arg(panel.flow.modelName) : ""
            color: Theme.faint
            font.pixelSize: 13
        }

        Item {
            id: plot
            x: curvePanel.plotX
            y: curvePanel.plotY
            width: curvePanel.plotW
            height: curvePanel.plotH

            Rectangle {
                anchors.fill: parent
                color: "transparent"
                border.width: 1
                border.color: Theme.line
            }
            // The decades, at the log positions this chart's 20 Hz - 20 kHz axis puts them.
            Repeater {
                model: [100, 1000, 10000]
                delegate: Rectangle {
                    required property int modelData
                    x: plot.width * Math.log(modelData / 20) / Math.log(1000)
                    width: 1
                    height: plot.height
                    color: Theme.line
                }
            }
            Rectangle { // 0 dB
                y: plot.height / 2
                width: plot.width
                height: 1
                color: Theme.faint
            }

            Shape {
                anchors.fill: parent
                visible: panel.flow.available
                ShapePath {
                    strokeColor: "transparent"
                    fillColor: Theme.accentSoft
                    PathSvg { path: panel.flow.fillPath }
                }
                ShapePath {
                    strokeColor: Theme.accent
                    strokeWidth: 3
                    fillColor: "transparent"
                    capStyle: ShapePath.RoundCap
                    joinStyle: ShapePath.RoundJoin
                    PathSvg { path: panel.flow.curvePath }
                }
            }

            Text {
                anchors.centerIn: parent
                width: parent.width - 40
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                visible: !panel.flow.available
                text: panel.model.note !== "" ? panel.model.note : qsTr("Waiting for the engine…")
                color: Theme.dim
                font.pixelSize: Theme.fontNormal
            }
        }

        // The axis: the range the curve was drawn in, and the frequencies it spans.
        Text {
            anchors.right: plot.left
            anchors.rightMargin: 10
            y: plot.y - 9
            text: "+" + panel.flow.dbTop + qsTr(" dB")
            color: Theme.dim
            font.pixelSize: 14
        }
        Text {
            anchors.right: plot.left
            anchors.rightMargin: 10
            y: plot.y + plot.height / 2 - 9
            text: "0 dB"
            color: Theme.dim
            font.pixelSize: 14
        }
        Text {
            anchors.right: plot.left
            anchors.rightMargin: 10
            y: plot.y + plot.height - 9
            text: panel.flow.dbBottom + qsTr(" dB")
            color: Theme.dim
            font.pixelSize: 14
        }
        Repeater {
            model: [{f: 20, label: "20 Hz", align: "left"}, {f: 100, label: "100", align: "center"},
                    {f: 1000, label: "1k", align: "center"}, {f: 10000, label: "10k", align: "before"},
                    {f: 20000, label: "20 kHz", align: "right"}]
            delegate: Text {
                required property var modelData
                readonly property real at: plot.width * Math.log(modelData.f / 20) / Math.log(1000)
                x: modelData.align === "left" ? plot.x
                   : modelData.align === "right" ? plot.x + plot.width - width
                   : modelData.align === "before" ? plot.x + at - width - 4
                   : plot.x + at - width / 2
                y: plot.y + plot.height + 4
                text: modelData.label
                color: Theme.dim
                font.pixelSize: 14
            }
        }
    }

    // --- the lite plugin's controls ------------------------------------------------------------
    Repeater {
        model: panel.model
        delegate: Item {
            id: control
            required property int index
            required property string param
            required property string kind
            required property string label
            required property real position
            required property string valueText
            required property int current
            readonly property var cell: panel.cells[param]
            readonly property bool highlighted: panel.active && panel.ring.highlight === index
            readonly property bool editing: highlighted && panel.ring.editing

            visible: cell !== undefined
            x: panel.gridX + (cell ? cell[0] : 0) * panel.cellWidth
            y: panel.gridY + (cell ? cell[1] : 0) * panel.cellHeight
            width: panel.cellWidth
            height: panel.cellHeight

            Knob {
                visible: control.kind === "knob"
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width
                diameter: 80
                position: control.position
                label: control.label
                valueText: control.valueText
                highlighted: control.highlighted
                editing: control.editing
                onPressed: panel.ring.highlight = control.index
                onMoved: (p) => panel.model.setPosition(control.index, p)
                onReleased: panel.model.release(control.index)
                onReset: panel.model.resetToDefault(control.index)
            }

            Column {
                visible: control.kind === "toggle"
                anchors.horizontalCenter: parent.horizontalCenter
                y: 34
                spacing: 10
                Toggle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    checked: control.current !== 0
                    highlighted: control.highlighted
                    onToggled: {
                        panel.ring.highlight = control.index
                        panel.model.choose(control.index, control.current ? 0 : 1)
                    }
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: control.label
                    color: Theme.dim
                    font.pixelSize: 14
                }
                // What Auto Gain is actually applying: the engine measures it, not the panel.
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: control.param === "auto_gain" && control.current !== 0 && panel.flow.autoGain
                    text: panel.flow.gainText
                    color: Theme.accent
                    font.pixelSize: Theme.fontSmall
                }
            }
        }
    }
}
