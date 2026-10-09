import QtQuick
import Rigpa.Core

// The view's title and the running rig; in rig view also Add, Undo and Redo.
Rectangle {
    color: Theme.background

    readonly property bool inRigView: App.nav.mainView === Navigator.RigView

    Text {
        id: title
        anchors.left: parent.left
        anchors.leftMargin: 24
        anchors.verticalCenter: parent.verticalCenter
        color: Theme.text
        font.pixelSize: Theme.fontTitle
        font.weight: Font.DemiBold
        text: App.nav.mainView === Navigator.BrowseView ? qsTr("Browse")
            : App.nav.mainView === Navigator.SettingsView ? qsTr("Settings")
            : qsTr("Rig")
    }

    Text {
        anchors.left: title.right
        anchors.leftMargin: 20
        anchors.baseline: title.baseline
        color: App.engine.hasRig ? Theme.text : Theme.dim
        font.pixelSize: Theme.fontLarge
        elide: Text.ElideRight
        width: Math.min(implicitWidth, 900)
        text: App.engine.connection !== EngineSession.Connected ? "\u2014"
            : App.engine.hasRig ? (App.engine.rigName !== "" ? App.engine.rigName : qsTr("(unnamed rig)"))
            : qsTr("no rig running")
    }

    Row {
        anchors.right: parent.right
        anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8
        visible: parent.inRigView
        HeaderButton { label: "+"; enabled: false }
        HeaderButton { label: qsTr("Undo"); enabled: false }
        HeaderButton { label: qsTr("Redo"); enabled: false }
    }

    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.line }
}
