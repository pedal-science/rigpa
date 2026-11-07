import QtQuick
import Rigpa
import Rigpa.Core

// The compact layout (the box's 4.3" panel, 800×480): a top bar, one view at a time, and a bottom
// bar saying what apply and back do. The play view is home and every other view is one step from
// it: back, or the top bar's "‹ Play". The overlays open over whichever view opened them, as on
// the bar (docs/spec.md, "The compact layout").
Item {
    id: shell

    Rectangle { anchors.fill: parent; color: Theme.background }

    CompactTopBar {
        width: parent.width
        height: Theme.headerHeight
    }

    Item {
        id: content
        y: Theme.headerHeight
        width: parent.width
        height: Theme.contentHeight
        clip: true
        // While an overlay is open, the view under it takes no input.
        readonly property bool overlaid: App.nodeEditor.isOpen || App.picker.isOpen || App.palette.isOpen
                                         || App.manager.dialog !== ""

        PlayView {
            anchors.fill: parent
            enabled: !content.overlaid
            visible: App.nav.mainView === Navigator.PlayView
        }
        RigsView {
            anchors.fill: parent
            enabled: !content.overlaid
            visible: App.nav.mainView === Navigator.RigsView
        }
        EditView {
            anchors.fill: parent
            enabled: !content.overlaid
            visible: App.nav.mainView === Navigator.RigView
        }
        // While it is the view, the engine runs a tuner node after each chain's input and the rig
        // is silent (docs/tuner.md), exactly as on the bar.
        CompactTunerView {
            anchors.fill: parent
            enabled: !content.overlaid
            visible: App.nav.mainView === Navigator.TunerView
        }
        CompactSettingsView {
            anchors.fill: parent
            enabled: !content.overlaid
            visible: App.nav.mainView === Navigator.SettingsView
        }

        // Overlays, over whichever view opened them.
        NodeEditor {
            anchors.fill: parent
            visible: App.nodeEditor.isOpen && !App.nodeEditor.isFlowEq
        }
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
