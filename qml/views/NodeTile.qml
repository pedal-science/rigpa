import QtQuick
import Rigpa

// One thing in the rig editor: a block, a parallel section's frame, a chain's In or Out, or a
// chain's label. Placed by RigEditorModel; drawn here.
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
    property bool selected: false
    property bool strong: true
    signal tapped(int count)

    x: itemX; y: itemY; width: itemW; height: itemH
    z: kind === "section" ? 0 : 1

    // A chain's label.
    Row {
        visible: tile.kind === "label"
        spacing: 12
        Text { text: tile.title; color: Theme.text; font.pixelSize: Theme.fontNormal; font.weight: Font.DemiBold }
        Text { text: tile.subtitle; color: Theme.dim; font.pixelSize: Theme.fontSmall; anchors.baseline: parent.children[0].baseline }
    }

    // A parallel section: a faint frame behind its lanes, selectable by its own strip.
    Rectangle {
        visible: tile.kind === "section"
        anchors.fill: parent
        radius: 10
        color: Qt.rgba(1, 1, 1, 0.025)
        border.width: tile.selected ? (tile.strong ? Theme.highlightWidth : 2) : 1
        border.color: tile.selected ? (tile.strong ? Theme.accent : Qt.darker(Theme.accent, 1.6)) : Theme.line
    }

    // In, Out and blocks.
    Rectangle {
        visible: tile.kind === "io" || tile.kind === "block"
        anchors.fill: parent
        radius: 12
        opacity: tile.bypassed ? 0.5 : 1
        color: tile.kind !== "block" || tile.tint === "" ? Theme.raised
             : Qt.tint(Theme.raised, Qt.rgba(Qt.color(tile.tint).r, Qt.color(tile.tint).g, Qt.color(tile.tint).b, 0.22))
        border.width: tile.selected ? (tile.strong ? Theme.highlightWidth : 2) : tile.missing ? 2 : 0
        border.color: tile.selected ? (tile.strong ? Theme.accent : Qt.darker(Theme.accent, 1.6)) : Theme.warn

        Rectangle { // the kind's colour, along the top
            visible: tile.kind === "block"
            x: 10; y: 8
            width: 26; height: 4; radius: 2
            color: tile.tint === "" ? Theme.faint : tile.tint
        }
        Column {
            x: 10
            y: tile.kind === "io" ? (parent.height - height) / 2 : 18
            width: parent.width - 20
            spacing: 2
            Text {
                width: parent.width
                text: tile.title
                color: Theme.text
                font.pixelSize: tile.kind === "io" ? Theme.fontSmall : 17
                lineHeight: 0.95
                horizontalAlignment: tile.kind === "io" ? Text.AlignHCenter : Text.AlignLeft
                elide: Text.ElideRight
                maximumLineCount: tile.kind === "io" ? 1 : 2
                wrapMode: Text.Wrap
            }
            Text {
                visible: text !== ""
                width: parent.width
                text: tile.missing ? qsTr("missing on the pedal") : tile.bypassed ? qsTr("bypassed") : tile.subtitle
                color: tile.missing ? Theme.warn : Theme.dim
                font.pixelSize: 13
                elide: Text.ElideRight
            }
        }
    }

    TapHandler {
        enabled: tile.select >= 0
        onTapped: tile.tapped(tapCount)
    }
}
