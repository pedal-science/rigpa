import QtQuick
import Rigpa

// One thing in the rig editor: a block, a parallel section's frame, a chain's In or Out, or a
// chain's label. Placed by RigEditorModel; drawn here, and the source of its touches and drags.
Item {
    id: tile
    required property string kind
    required property real itemX
    required property real itemY
    required property real itemW
    required property real itemH
    required property string title
    required property string subtitle
    required property string tint
    required property bool bypassed
    required property bool missing
    required property int select
    required property string element
    required property int lift
    property bool selected: false
    property bool strong: true
    signal tapped(int count)
    signal liftAt(point p)
    signal dragAt(point p)
    signal dropAt(point p)
    signal dragCanceled()

    x: itemX; y: itemY; width: itemW; height: itemH
    z: kind === "section" ? 0 : 1
    visible: lift !== 2
    // Tiles stepping aside for a dragged one, and rows re-wrapping, glide rather than jump.
    Behavior on x { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
    Behavior on y { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }

    // A chain's label.
    Row {
        visible: tile.kind === "label"
        spacing: 12
        Text { text: tile.title; color: Theme.text; font.pixelSize: Theme.fontNormal; font.weight: Font.DemiBold }
        Text { text: tile.subtitle; color: Theme.dim; font.pixelSize: Theme.fontSmall; anchors.baseline: parent.children[0].baseline }
    }

    // A parallel section: a faint frame behind its lanes, selectable by its own strip.
    Rectangle {
        visible: tile.kind === "section" && tile.lift === 0
        anchors.fill: parent
        radius: 10
        color: Qt.rgba(1, 1, 1, 0.025)
        border.width: tile.selected ? (tile.strong ? Theme.highlightWidth : 2) : 1
        border.color: tile.selected ? (tile.strong ? Theme.accent : Qt.darker(Theme.accent, 1.6)) : Theme.line
    }

    // In, Out and blocks; a lifted block leaves a gap where it will land.
    TileFace {
        visible: (tile.kind === "io" || tile.kind === "block") && tile.lift === 0
        anchors.fill: parent
        kind: tile.kind
        title: tile.title
        subtitle: tile.subtitle
        tint: tile.tint
        bypassed: tile.bypassed
        missing: tile.missing
        selected: tile.selected
        strong: tile.strong
    }
    Rectangle { // the gap
        visible: tile.lift === 1
        anchors.fill: parent
        radius: 12
        color: Theme.accentSoft
        border.width: 2
        border.color: Qt.darker(Theme.accent, 1.3)
    }

    // A tap selects; a double tap bypasses; a long press (or, with a mouse, a drag) lifts the
    // block so it can be moved. Points go out in the editor's coordinates, because the tile
    // itself moves while it is dragged.
    MouseArea {
        id: pointer
        anchors.fill: parent
        enabled: tile.select >= 0
        pressAndHoldInterval: 400
        property bool dragging: false
        property bool mouse: false
        property point pressedAt
        function editorPoint(m) { return tile.mapToItem(tile.parent, m.x, m.y) }

        onPressed: (m) => {
            pressedAt = editorPoint(m)
            // A mouse or trackpad drags at once; a finger would scroll, so it lifts on a long press.
            mouse = m.source === Qt.MouseEventNotSynthesized
            preventStealing = mouse
        }
        onPressAndHold: (m) => {
            if (dragging)
                return
            dragging = true
            preventStealing = true
            tile.liftAt(editorPoint(m))
        }
        onPositionChanged: (m) => {
            const p = editorPoint(m)
            if (!dragging && mouse && Math.hypot(p.x - pressedAt.x, p.y - pressedAt.y) > 8) {
                dragging = true
                tile.liftAt(pressedAt)
            }
            if (dragging)
                tile.dragAt(p)
        }
        onReleased: (m) => {
            if (!dragging)
                return
            dragging = false
            preventStealing = false
            tile.dropAt(editorPoint(m))
        }
        onCanceled: {
            if (dragging) {
                dragging = false
                tile.dragCanceled()
            }
        }
        onClicked: if (!dragging) tile.tapped(1)
        onDoubleClicked: tile.tapped(2)
    }
}
