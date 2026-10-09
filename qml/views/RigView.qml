import QtQuick
import Rigpa
import Rigpa.Core

// | rig list | rig editor |. The list's width is a setting (settings view), not a splitter.
// Rigs and nodes arrive in M4; until then both regions show where the hardware focus is.
Item {
    id: view
    readonly property bool listFocused: App.nav.rigFocus === Navigator.RigListFocus
                                        && App.nav.overlay === Navigator.NoOverlay

    Panel {
        id: rigList
        x: 12; y: 12
        width: App.settings.rigListWidth - 18
        height: parent.height - 24
        focused: view.listFocused
        Text {
            anchors.centerIn: parent
            width: parent.width - 32
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: qsTr("Rig list\n(rigs arrive in M4)")
            color: Theme.dim
            font.pixelSize: Theme.fontNormal
        }
        TapHandler { onTapped: App.nav.focusRigList() }
    }

    Panel {
        x: App.settings.rigListWidth + 6
        y: 12
        width: parent.width - x - 12
        height: parent.height - 24
        focused: !view.listFocused && App.nav.overlay === Navigator.NoOverlay
        Text {
            anchors.centerIn: parent
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Rig editor\n(nodes arrive in M4)")
            color: Theme.dim
            font.pixelSize: Theme.fontNormal
        }
        TapHandler { onTapped: App.nav.focusRigEditor() }
    }
}
