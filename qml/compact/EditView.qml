import QtQuick
import Rigpa
import Rigpa.Core

// The compact layout's chain editor: the running rig's tiles at the panel's size (LayoutSpec's
// editor style), with +, Undo and Redo in the top bar. A tap selects a tile, a second tap opens
// its knobs (the node editor, over this view), a double tap switches it, and a long press lifts it
// to move or to drop on the trash: the rig editor of the bar, in the panel's space.
Item {
    id: view
    readonly property bool idle: App.nav.overlay === Navigator.NoOverlay

    RigEditorPanel {
        anchors.fill: parent
        anchors.margins: 6
        focused: view.idle
    }
}
