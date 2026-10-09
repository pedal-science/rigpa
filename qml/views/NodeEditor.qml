import QtQuick
import Rigpa
import Rigpa.Core

// The node editor: over the whole content area, every parameter of the selected block at once,
// sized so that it all fits without scrolling. The title row is [Remove] … [Bypass][Close]; the
// header above says which block this is. Controls come from engine.schema (NodeEditorModel).
Rectangle {
    id: editor
    readonly property var model: App.nodeEditor
    readonly property FocusRing ring: model.ring
    readonly property bool active: App.nav.overlay === Navigator.NodeEditor
    color: Theme.background

    // A grid that never scrolls: each control takes a number of cells, and the cells shrink until
    // the controls, packed in reading order, fit the area.
    readonly property real baseCellW: 128
    readonly property real baseCellH: 140
    property real scale: 1
    property var spans: []

    function spanOf(kind, options) {
        if (kind === "picker")
            return 3
        if (kind === "choice")
            return Math.max(2, Math.ceil(options.length * 0.6))
        return 1
    }
    function relayout() {
        const s = []
        for (let i = 0; i < repeater.count; ++i) {
            const item = repeater.itemAt(i)
            s.push(item ? item.span : 1)
        }
        let scale = 1.25
        for (; scale > 0.5; scale -= 0.05) {
            const perRow = Math.floor(controls.width / (baseCellW * scale))
            let rows = 1, used = 0
            for (const span of s) {
                if (used + span > perRow) { rows++; used = 0 }
                used += span
            }
            if (rows * baseCellH * scale <= controls.height)
                break
        }
        editor.scale = scale
    }
    Connections {
        target: editor.model
        function onModelReset() { Qt.callLater(editor.relayout) }
    }
    onWidthChanged: Qt.callLater(relayout)
    onHeightChanged: Qt.callLater(relayout)

    // [Remove] … [Bypass][Close]
    Item {
        id: titleRow
        x: 24; y: 10
        width: parent.width - 48
        height: 48

        HeaderButton {
            anchors.verticalCenter: parent.verticalCenter
            label: qsTr("Remove")
            enabled: false // M7: removing nodes
        }
        Text {
            anchors.centerIn: parent
            width: parent.width - 520
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            text: editor.model.subtitle
            color: Theme.dim
            font.pixelSize: Theme.fontNormal
        }
        Row {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: 14
            Text {
                visible: editor.model.bypassable
                anchors.verticalCenter: parent.verticalCenter
                text: editor.model.bypassed ? qsTr("Bypassed") : qsTr("Active")
                color: editor.model.bypassed ? Theme.dim : Theme.text
                font.pixelSize: Theme.fontNormal
            }
            Toggle { // on = the block is in the signal path
                visible: editor.model.bypassable
                anchors.verticalCenter: parent.verticalCenter
                checked: !editor.model.bypassed
                highlighted: editor.active && editor.ring.highlight === editor.model.bypassIndex
                onToggled: { editor.ring.highlight = editor.model.bypassIndex; editor.model.toggleBypass() }
            }
            Item { width: 10; height: 1 }
            HeaderButton {
                anchors.verticalCenter: parent.verticalCenter
                label: qsTr("Close")
                highlighted: editor.active && editor.ring.highlight === editor.model.closeIndex
                onClicked: App.closeNodeEditor()
            }
        }
    }

    Item {
        id: controls
        x: 24
        y: titleRow.y + titleRow.height + 12
        width: parent.width - 48
        height: parent.height - y - 8

        Flow {
            width: parent.width
            Repeater {
                id: repeater
                model: editor.model
                onCountChanged: Qt.callLater(editor.relayout)

                delegate: Item {
                    id: cell
                    required property int index
                    required property string kind
                    required property string label
                    required property string group
                    required property bool firstOfGroup
                    required property real position
                    required property string valueText
                    required property var options
                    required property int current
                    readonly property int span: editor.spanOf(kind, options)
                    readonly property bool highlighted: editor.active && editor.ring.highlight === index
                    readonly property bool editing: highlighted && editor.ring.editing

                    width: span * editor.baseCellW * editor.scale
                    height: editor.baseCellH * editor.scale

                    Rectangle { // a line between one node's controls and the next's
                        visible: cell.firstOfGroup && cell.index > 0
                        x: 0; y: 4
                        width: 1; height: parent.height - 8
                        color: Theme.line
                    }
                    Text {
                        visible: cell.firstOfGroup
                        x: 10; y: 0
                        width: parent.width * 3 // a group's name may run over its later controls
                        text: cell.group
                        color: Theme.faint
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        font.capitalization: Font.AllUppercase
                    }

                    Item {
                        id: body
                        y: 20 * editor.scale
                        width: parent.width
                        height: parent.height - y

                        Knob {
                            visible: cell.kind === "knob"
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: parent.width
                            diameter: 70 * editor.scale
                            position: cell.position
                            label: cell.label
                            valueText: cell.valueText
                            highlighted: cell.highlighted
                            editing: cell.editing
                            onPressed: editor.ring.highlight = cell.index
                            onMoved: (p) => editor.model.setPosition(cell.index, p)
                            onReleased: editor.model.release(cell.index)
                        }

                        Column {
                            visible: cell.kind === "choice" || cell.kind === "toggle"
                            anchors.centerIn: parent
                            spacing: 10
                            Segmented {
                                visible: cell.kind === "choice"
                                anchors.horizontalCenter: parent.horizontalCenter
                                labels: cell.options
                                current: cell.current
                                segmentWidth: 56
                                fontSize: Theme.fontSmall
                                highlighted: cell.highlighted
                                onChosen: (i) => { editor.ring.highlight = cell.index; editor.model.choose(cell.index, i) }
                            }
                            Toggle {
                                visible: cell.kind === "toggle"
                                anchors.horizontalCenter: parent.horizontalCenter
                                checked: cell.current !== 0
                                highlighted: cell.highlighted
                                onToggled: { editor.ring.highlight = cell.index; editor.model.choose(cell.index, cell.current ? 0 : 1) }
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: cell.label
                                color: Theme.dim
                                font.pixelSize: 14
                            }
                        }

                        Column { // a model or IR: the name, and a press opens the picker
                            visible: cell.kind === "picker"
                            anchors.centerIn: parent
                            width: parent.width - 20
                            spacing: 8
                            Rectangle {
                                width: parent.width
                                height: 64 * editor.scale + 8
                                radius: 10
                                color: pickTap.pressed ? Theme.panel : Theme.raised
                                border.width: cell.highlighted ? Theme.highlightWidth : 0
                                border.color: Theme.accent
                                Text {
                                    x: 14
                                    width: parent.width - 48
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: cell.valueText
                                    color: Theme.text
                                    font.pixelSize: 17
                                    wrapMode: Text.Wrap
                                    maximumLineCount: 2
                                    elide: Text.ElideRight
                                }
                                Text {
                                    anchors.right: parent.right
                                    anchors.rightMargin: 14
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: "›"
                                    color: Theme.dim
                                    font.pixelSize: 30
                                }
                                TapHandler {
                                    id: pickTap
                                    onTapped: { editor.ring.highlight = cell.index; editor.model.ring.activated(cell.index) }
                                }
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: cell.label
                                color: Theme.dim
                                font.pixelSize: 14
                            }
                        }
                    }
                }
            }
        }

        Text {
            anchors.centerIn: parent
            width: parent.width * 0.7
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            visible: editor.model.note !== ""
            text: editor.model.note
            color: Theme.dim
            font.pixelSize: Theme.fontNormal
        }
    }
}
