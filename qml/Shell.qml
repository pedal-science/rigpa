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
            // While an overlay is open, the view under it takes no input.
            readonly property bool overlaid: App.nodeEditor.isOpen || App.picker.isOpen || App.palette.isOpen
                                             || App.manager.dialog !== ""

            RigView {
                anchors.fill: parent
                enabled: !content.overlaid
                visible: App.nav.mainView === Navigator.RigView
            }
            BrowseView {
                anchors.fill: parent
                enabled: !content.overlaid
                visible: App.nav.mainView === Navigator.BrowseView
            }
            SettingsView {
                anchors.fill: parent
                enabled: !content.overlaid
                visible: App.nav.mainView === Navigator.SettingsView
            }
            // Overlays, over whichever view opened them.
            NodeEditor {
                anchors.fill: parent
                visible: App.nodeEditor.isOpen && !App.nodeEditor.isFlowEq
            }
            // flowEQ's block has its own layout: its controls and the response curve the engine
            // computed for it (FlowEqPanel.qml, FlowEqModel).
            FlowEqPanel {
                anchors.fill: parent
                visible: App.nodeEditor.isOpen && App.nodeEditor.isFlowEq
            }
            Picker {
                anchors.fill: parent
                visible: App.picker.isOpen
            }
            AddPanel {
                anchors.fill: parent
                visible: App.palette.isOpen
            }
            RigDialog {
                anchors.fill: parent
                visible: App.manager.dialog !== ""
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
