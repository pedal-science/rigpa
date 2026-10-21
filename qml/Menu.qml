import QtQuick
import Rigpa.Core

// Five 96×96 slots, top to bottom: rig view (blank while it is the active main view), browse,
// a spare, the tuner, settings. The tuner is the one view that changes what the engine runs -- it
// mounts a node while it is up -- so its button is disabled on an engine that has no tuner node
// (namchain docs/tuner.md).
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
        MenuButton {
            icon: Theme.icon("tuner")
            active: App.nav.mainView === Navigator.TunerView
            enabled: App.engine.hasTuner
            onClicked: App.nav.showMainView(Navigator.TunerView)
        }
        MenuButton {
            icon: Theme.icon("settings")
            active: App.nav.mainView === Navigator.SettingsView
            onClicked: App.nav.showMainView(Navigator.SettingsView)
        }
    }

    Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: Theme.line }
}
