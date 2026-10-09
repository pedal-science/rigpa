import QtQuick
import Rigpa

// How a block or a chain's In or Out looks: its tint, its name and what it is. Drawn by the tile
// in the rig editor, and by the ghost that follows the pointer while a block is dragged.
Rectangle {
    id: face
    property string kind: "block"
    property string title
    property string subtitle
    property string tint
    property bool bypassed: false
    property bool missing: false
    property bool selected: false
    property bool strong: true

    radius: 12
    opacity: face.bypassed ? 0.5 : 1
    color: face.kind !== "block" || face.tint === "" ? Theme.raised
         : Qt.tint(Theme.raised, Qt.rgba(Qt.color(face.tint).r, Qt.color(face.tint).g, Qt.color(face.tint).b, 0.22))
    border.width: face.selected ? (face.strong ? Theme.highlightWidth : 2) : face.missing ? 2 : 0
    border.color: face.selected ? (face.strong ? Theme.accent : Qt.darker(Theme.accent, 1.6)) : Theme.warn

    Rectangle { // the kind's colour, along the top
        visible: face.kind === "block"
        x: 10; y: 8
        width: 26; height: 4; radius: 2
        color: face.tint === "" ? Theme.faint : face.tint
    }
    Column {
        x: 10
        y: face.kind === "io" ? (parent.height - height) / 2 : 18
        width: parent.width - 20
        spacing: 2
        Text {
            width: parent.width
            text: face.title
            color: Theme.text
            font.pixelSize: face.kind === "io" ? Theme.fontSmall : 17
            lineHeight: 0.95
            horizontalAlignment: face.kind === "io" ? Text.AlignHCenter : Text.AlignLeft
            elide: Text.ElideRight
            maximumLineCount: face.kind === "io" ? 1 : 2
            wrapMode: Text.Wrap
        }
        Text {
            visible: text !== ""
            width: parent.width
            text: face.missing ? qsTr("missing on the pedal") : face.bypassed ? qsTr("bypassed") : face.subtitle
            color: face.missing ? Theme.warn : Theme.dim
            font.pixelSize: 13
            elide: Text.ElideRight
        }
    }
}
