import QtQuick
import Rigpa
import Rigpa.Core

// The node editor's title row, shared by the generic editor and by flowEQ's own panel:
// [Remove] … [Bypass][Close], with the block's summary in the middle and the hardware highlight on
// whichever of them the ring is on.
Item {
    id: row
    // The node editor model; every index and label here comes from it (NodeEditorModel).
    property var model
    // The editor is the open overlay, so its ring is the one the hardware is moving.
    property bool active: false

    height: 48

    HeaderButton {
        anchors.verticalCenter: parent.verticalCenter
        label: qsTr("Remove")
        highlighted: row.active && row.model.ring.highlight === row.model.removeIndex
        onClicked: row.model.removeElement()
    }
    Text {
        anchors.centerIn: parent
        width: parent.width - 520
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
        text: row.model.subtitle
        color: Theme.dim
        font.pixelSize: Theme.fontNormal
    }
    Row {
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        spacing: 14
        Text {
            visible: row.model.bypassable
            anchors.verticalCenter: parent.verticalCenter
            text: row.model.bypassed ? qsTr("Bypassed") : qsTr("Active")
            color: row.model.bypassed ? Theme.dim : Theme.text
            font.pixelSize: Theme.fontNormal
        }
        Toggle { // on = the block is in the signal path
            visible: row.model.bypassable
            anchors.verticalCenter: parent.verticalCenter
            checked: !row.model.bypassed
            highlighted: row.active && row.model.ring.highlight === row.model.bypassIndex
            onToggled: {
                row.model.ring.highlight = row.model.bypassIndex
                row.model.toggleBypass()
            }
        }
        Item { width: 10; height: 1 }
        HeaderButton {
            anchors.verticalCenter: parent.verticalCenter
            label: qsTr("Close")
            highlighted: row.active && row.model.ring.highlight === row.model.closeIndex
            onClicked: App.closeNodeEditor()
        }
    }
}
