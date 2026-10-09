import QtQuick
import Rigpa
import Rigpa.Core

// The running rig: each chain's tiles from In to Out, wrapped into rows, parallel sections with
// their lanes, and the wires between. One block or section is selected (the rotary moves it in
// signal order; a tap picks one). Drag scrolls when the rig is taller than the panel.
Panel {
    id: panel
    readonly property FocusRing ring: App.nav.rigEditor
    readonly property var model: App.editor
    property string selectedElement: ""
    clip: true

    onWidthChanged: model.availableWidth = width
    Component.onCompleted: { model.availableWidth = width; sync() }

    function sync() {
        ring.count = model.selectableCount
        // A re-read keeps the selection on the same element where it still exists.
        const kept = model.selectOf(selectedElement)
        if (kept >= 0)
            ring.highlight = kept
        selectedElement = model.elementAt(ring.highlight)
    }
    Connections {
        target: panel.model
        function onLaidOut() { panel.sync() }
    }
    Connections {
        target: panel.ring
        function onHighlightChanged() {
            panel.selectedElement = panel.model.elementAt(panel.ring.highlight)
            panel.reveal(panel.model.rectOf(panel.ring.highlight))
        }
    }

    // Scroll just enough to show a tile, with a short animation.
    function reveal(r) {
        if (r.height <= 0)
            return
        let y = flick.contentY
        if (r.y < y + 8)
            y = r.y - 8
        else if (r.y + r.height > y + flick.height - 8)
            y = r.y + r.height - flick.height + 8
        y = Math.max(0, Math.min(y, flick.contentHeight - flick.height))
        if (Math.abs(y - flick.contentY) > 1) {
            scroll.to = y
            scroll.restart()
        }
    }
    Timer {
        id: openLater
        property int select: -1
        interval: 300
        onTriggered: App.openNodeEditor(select)
    }

    NumberAnimation { id: scroll; target: flick; property: "contentY"; duration: 160; easing.type: Easing.OutCubic }

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: panel.model.contentWidth
        contentHeight: panel.model.contentHeight
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.VerticalFlick

        Repeater {
            model: panel.model.wires
            delegate: Rectangle {
                required property rect modelData
                x: modelData.x; y: modelData.y
                width: modelData.width; height: modelData.height
                color: Theme.faint
            }
        }
        Repeater {
            model: panel.model
            delegate: NodeTile {
                selected: select >= 0 && select === panel.ring.highlight
                strong: panel.focused
                // A tap selects; a tap on the node already selected opens its editor, once the
                // double-tap window has passed; a double tap toggles bypass.
                onTapped: (count) => {
                    if (select < 0)
                        return
                    App.nav.focusRigEditor()
                    if (count >= 2) {
                        openLater.stop()
                        App.rig.toggleBypass(element)
                        return
                    }
                    const wasSelected = panel.ring.highlight === select
                    panel.ring.highlight = select
                    if (wasSelected) {
                        openLater.select = select
                        openLater.restart()
                    }
                }
            }
        }
    }

    Text {
        anchors.centerIn: parent
        visible: panel.model.contentHeight === 0
        text: App.engine.connection !== EngineSession.Connected ? qsTr("Not connected to the engine")
            : App.engine.hasRig ? qsTr("Reading the rig…") : qsTr("No rig running")
        color: Theme.dim
        font.pixelSize: Theme.fontNormal
    }
}
