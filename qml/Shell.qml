import QtQuick
import Rigpa.Core

// | menu | main |, where main is header / content / footer beside one full-height info panel.
Item {
    id: shell

    Rectangle { anchors.fill: parent; color: Theme.background }

    Menu {
        id: menu
        width: Theme.menuWidth
        height: parent.height
    }

    Item {
        id: main
        x: Theme.menuWidth
        width: Theme.mainWidth
        height: parent.height

        Header { id: header; width: parent.width; height: Theme.headerHeight }

        Item {
            id: content
            y: Theme.headerHeight
            width: parent.width
            height: Theme.contentHeight
            clip: true

            RigView {
                anchors.fill: parent
                visible: App.nav.mainView === Navigator.RigView
            }
            BrowseView {
                anchors.fill: parent
                visible: App.nav.mainView === Navigator.BrowseView
            }
            SettingsView {
                anchors.fill: parent
                visible: App.nav.mainView === Navigator.SettingsView
            }
        }

        Footer {
            y: Theme.headerHeight + Theme.contentHeight
            width: parent.width
            height: Theme.footerHeight
        }
    }

    InfoPanel {
        x: Theme.menuWidth + Theme.mainWidth
        width: Theme.infoWidth
        height: parent.height
    }
}
