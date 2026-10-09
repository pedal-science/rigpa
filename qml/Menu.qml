import QtQuick
import Rigpa.Core

// Five 96×96 slots, top to bottom: rig view (blank while it is the active main view), browse,
// two spare, settings.
Rectangle {
    color: Theme.menuBackground

    Column {
        MenuButton {
            icon: App.nav.mainView === Navigator.RigView ? "" : Theme.icon("rigpa")
            enabled: App.nav.mainView !== Navigator.RigView
            onClicked: App.nav.showMainView(Navigator.RigView)
        }
        MenuButton {
            icon: Theme.icon("folder")
            active: App.nav.mainView === Navigator.BrowseView
            onClicked: App.nav.showMainView(Navigator.BrowseView)
        }
        MenuButton { enabled: false }
        MenuButton { enabled: false }
        MenuButton {
            icon: Theme.icon("settings")
            active: App.nav.mainView === Navigator.SettingsView
            onClicked: App.nav.showMainView(Navigator.SettingsView)
        }
    }

    Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: Theme.line }
}
