import QtQuick
import Rigpa
import Rigpa.Core

// | rig list | rig editor |. The list's width is a setting (settings view), not a splitter.
Item {
    id: view
    readonly property bool idle: App.nav.overlay === Navigator.NoOverlay
    readonly property bool listFocused: App.nav.rigFocus === Navigator.RigListFocus && idle

    RigListPanel {
        x: 12; y: 12
        width: App.settings.rigListWidth - 18
        height: parent.height - 24
        focused: view.listFocused
    }

    RigEditorPanel {
        x: App.settings.rigListWidth + 6
        y: 12
        width: parent.width - x - 12
        height: parent.height - 24
        focused: !view.listFocused && view.idle
    }
}
