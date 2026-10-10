import QtQuick
import Rigpa.Core

// The view's title and the running rig; in rig view also Add, Undo and Redo.
Rectangle {
    color: Theme.background

    readonly property bool inRigView: App.nav.mainView === Navigator.RigView
    // The node editor names its block here; the rig is still running underneath.
    readonly property bool editingNode: App.nodeEditor.isOpen

    Text {
        id: title
        anchors.left: parent.left
        anchors.leftMargin: 24
        anchors.verticalCenter: parent.verticalCenter
        color: Theme.text
        font.pixelSize: Theme.fontTitle
        font.weight: Font.DemiBold
        width: Math.min(implicitWidth, 700)
        elide: Text.ElideRight
        text: parent.editingNode ? App.nodeEditor.title
            : App.nav.mainView === Navigator.BrowseView ? qsTr("Browse")
            : App.nav.mainView === Navigator.SettingsView ? qsTr("Settings")
            : qsTr("Rig")
    }

    Text {
        id: rigName
        anchors.left: title.right
        anchors.leftMargin: 20
        anchors.baseline: title.baseline
        color: App.engine.hasRig && !parent.editingNode ? Theme.text : Theme.dim
        font.pixelSize: Theme.fontLarge
        elide: Text.ElideRight
        width: Math.min(implicitWidth, 600)
        text: App.engine.connection !== EngineSession.Connected ? "\u2014"
            : App.engine.hasRig ? (App.engine.rigName !== "" ? App.engine.rigName : qsTr("(unnamed rig)"))
            : qsTr("no rig running")
    }

    // Whether the library has what is playing: saved, on its way, or not in the library at all.
    Text {
        id: saveMark
        anchors.left: rigName.right
        anchors.leftMargin: 14
        anchors.baseline: title.baseline
        visible: parent.inRigView && !parent.editingNode && text !== ""
        font.pixelSize: Theme.fontSmall
        readonly property string state: App.manager.saveState
        color: state === "unsaved" || state === "failed" ? Theme.warn : Theme.dim
        text: state === "saved" ? qsTr("saved")
            : state === "pending" || state === "saving" ? qsTr("saving…")
            : state === "unsaved" ? qsTr("not in the library")
            : state === "failed" ? qsTr("not saved") : ""
    }

    Row {
        anchors.right: parent.right
        anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8
        visible: parent.inRigView && !parent.editingNode && !App.palette.isOpen
        HeaderButton { label: "+"; enabled: App.rig.hasDocument && !App.palette.isOpen; onClicked: App.openPalette() }
        HeaderButton { label: qsTr("Undo"); enabled: App.rig.canUndo; onClicked: App.rig.undo() }
        HeaderButton { label: qsTr("Redo"); enabled: App.rig.canRedo; onClicked: App.rig.redo() }
    }

    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.line }
}
