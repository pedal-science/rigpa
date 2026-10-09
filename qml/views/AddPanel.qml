import QtQuick
import Rigpa
import Rigpa.Core

// Adding to the rig (the header's +): an amp, a parallel split, or any node type the engine
// offers. What is chosen goes in after the selected block. The rotary moves through the list,
// apply or a tap chooses, back closes.
Rectangle {
    id: adder
    readonly property var model: App.palette
    readonly property FocusRing ring: model.ring
    readonly property bool active: App.nav.overlay === Navigator.Palette
    color: Theme.background

    Connections {
        target: adder.ring
        function onHighlightChanged() { if (adder.visible) list.positionViewAtIndex(adder.ring.highlight, ListView.Contain) }
    }

    Item {
        id: titleRow
        x: 24; y: 10
        width: parent.width - 48
        height: 48
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Add to the rig")
            color: Theme.text
            font.pixelSize: Theme.fontLarge
        }
        HeaderButton {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            label: qsTr("Close")
            onClicked: App.nav.closeOverlay()
        }
    }

    Panel {
        x: 24
        y: titleRow.y + titleRow.height + 8
        width: parent.width - 48
        height: parent.height - y - 10
        clip: true

        // Wide rows in columns: the whole palette fits the screen without scrolling.
        GridView {
            id: list
            anchors.fill: parent
            anchors.margins: 6
            model: adder.model
            cellWidth: width / 4
            cellHeight: 62
            boundsBehavior: Flickable.StopAtBounds
            delegate: Rectangle {
                id: row
                required property int index
                required property string name
                required property string detail
                required property string entryKind
                readonly property bool highlighted: adder.active && adder.ring.highlight === index
                width: GridView.view.cellWidth - 6
                height: GridView.view.cellHeight - 6
                radius: 8
                color: tap.pressed ? Theme.raised : entryKind === "type" ? "transparent" : Theme.accentSoft
                border.width: highlighted ? Theme.highlightWidth : 0
                border.color: Theme.accent
                Column {
                    x: 14
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - 28
                    spacing: 2
                    Text { width: parent.width; elide: Text.ElideRight; text: row.name; color: Theme.text; font.pixelSize: Theme.fontNormal }
                    Text { width: parent.width; elide: Text.ElideRight; text: row.detail; color: Theme.dim; font.pixelSize: Theme.fontSmall }
                }
                TapHandler {
                    id: tap
                    onTapped: { adder.ring.highlight = row.index; adder.model.choose(row.index) }
                }
            }
        }
    }
}
