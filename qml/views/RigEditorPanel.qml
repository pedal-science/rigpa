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
        interactive: !panel.model.dragging

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
                onLiftAt: (p) => panel.liftAt(element, select, p, Qt.point(itemX, itemY))
                onDragAt: (p) => panel.dragAt(p)
                onDropAt: (p) => panel.dropAt(p)
                onDragCanceled: panel.cancelDrag()
            }
        }

        // The lifted block, under the pointer.
        TileFace {
            id: ghost
            visible: panel.model.dragging
            z: 10
            x: panel.model.dragPoint.x - panel.grab.x
            y: panel.model.dragPoint.y - panel.grab.y
            width: 188
            height: 96
            opacity: 0.92
            selected: true
            scale: 1.04
        }
    }

    // Drag and drop (RigEditorModel does the placing): where the pointer is, in the panel's own
    // coordinates, so it stays put while the view auto-scrolls under it.
    property point pointer
    property point grab
    readonly property real edgeZone: Math.min(78, height / 4)

    function contentPoint() { return Qt.point(pointer.x + flick.contentX, pointer.y + flick.contentY) }
    function liftAt(element, select, p, tileAt) {
        openLater.stop()
        grab = Qt.point(Math.min(p.x - tileAt.x, ghost.width - 12), Math.min(p.y - tileAt.y, ghost.height - 12))
        pointer = Qt.point(p.x - flick.contentX, p.y - flick.contentY)
        if (!model.lift(element))
            return
        model.dragTo(p)
        ring.highlight = select
        App.nav.focusRigEditor()
    }
    function dragAt(p) {
        pointer = Qt.point(p.x - flick.contentX, p.y - flick.contentY)
        model.dragTo(contentPoint())
    }
    function dropAt(p) {
        pointer = Qt.point(p.x - flick.contentX, p.y - flick.contentY)
        if (trash.contains(trash.mapFromItem(panel, pointer.x, pointer.y)))
            model.dropOnTrash()
        else
            model.drop()
    }
    function cancelDrag() { model.cancelDrag() }
    Connections {
        target: panel.model
        function onDraggingChanged() {
            if (!panel.model.dragging)
                return
            const look = panel.model.describeElement(panel.model.liftedElement)
            ghost.kind = look.kind === "section" ? "block" : look.kind
            ghost.title = look.title
            ghost.subtitle = look.subtitle
            ghost.tint = look.tint
            ghost.bypassed = look.bypassed
        }
    }

    // Near the top or bottom edge the view scrolls, faster the deeper the pointer goes
    // (ljoss's AutoScroll: 900 px/s at the edge, by depth squared).
    Timer {
        interval: 16
        repeat: true
        running: panel.model.dragging
        onTriggered: {
            const y = panel.pointer.y
            let depth = 0
            if (y < panel.edgeZone)
                depth = -(panel.edgeZone - y) / panel.edgeZone
            else if (y > panel.height - panel.edgeZone)
                depth = (y - (panel.height - panel.edgeZone)) / panel.edgeZone
            if (depth === 0)
                return
            depth = Math.max(-1, Math.min(1, depth))
            const max = Math.max(0, flick.contentHeight - flick.height)
            const next = Math.max(0, Math.min(max, flick.contentY + 900 * 0.016 * depth * Math.abs(depth)))
            if (next !== flick.contentY) {
                flick.contentY = next
                panel.model.dragTo(panel.contentPoint())
            }
        }
    }

    // Drop here to take the block out of the rig.
    Rectangle {
        id: trash
        visible: panel.model.dragging
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 12
        width: 150
        height: 64
        radius: 12
        readonly property bool armed: visible && contains(mapFromItem(panel, panel.pointer.x, panel.pointer.y))
        color: armed ? Qt.rgba(0.88, 0.31, 0.31, 0.35) : Theme.raised
        border.width: 2
        border.color: armed ? Theme.bad : Theme.line
        Text {
            anchors.centerIn: parent
            text: qsTr("Remove")
            color: trash.armed ? Theme.text : Theme.dim
            font.pixelSize: Theme.fontNormal
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
