import QtQuick
import Rigpa
import Rigpa.Core

// The compact layout's top bar. On Play: the engine's state, since Play itself shows the rig.
// Elsewhere: "‹ Play" (touch's way home; the encoder's is back) and the view's title, and in the
// chain editor the rig's saved state with +, Undo and Redo. The engine's health and DSP load are
// always at the right, where the bar's info panel has them.
Rectangle {
    id: bar
    color: Theme.background

    readonly property int view: App.nav.mainView
    readonly property bool onPlay: view === Navigator.PlayView
    readonly property bool inEditor: view === Navigator.RigView
    readonly property bool editingNode: App.nodeEditor.isOpen
    readonly property bool connected: App.engine.connection === EngineSession.Connected

    function healthColor(h) { return h === "ok" ? Theme.good : h === "warn" ? Theme.warn : h === "error" ? Theme.bad : Theme.faint }

    HeaderButton {
        id: home
        visible: !bar.onPlay
        x: 8
        anchors.verticalCenter: parent.verticalCenter
        height: 40
        label: "‹ " + qsTr("Play")
        onClicked: App.nav.showMainView(Navigator.PlayView)
    }

    Text {
        id: title
        anchors.left: bar.onPlay ? parent.left : home.right
        anchors.leftMargin: bar.onPlay ? 20 : 14
        anchors.verticalCenter: parent.verticalCenter
        width: Math.min(implicitWidth, bar.inEditor ? 300 : 420)
        elide: Text.ElideRight
        color: bar.onPlay && !bar.connected ? Theme.warn : Theme.text
        font.pixelSize: bar.onPlay ? Theme.fontNormal : Theme.fontLarge
        font.weight: bar.onPlay ? Font.Normal : Font.DemiBold
        text: bar.editingNode ? App.nodeEditor.title
            // Play shows the rig itself; the top bar says only whether the engine is there (the
            // bottom bar has the detail of a connection problem).
            : bar.onPlay ? (bar.connected ? qsTr("Engine ") + (App.engine.state || qsTr("connected"))
                                          : qsTr("Engine not connected"))
            : bar.view === Navigator.RigsView ? qsTr("Rigs")
            : bar.view === Navigator.TunerView ? qsTr("Tuner")
            : bar.view === Navigator.SettingsView ? qsTr("Settings")
            : App.engine.hasRig ? (App.engine.rigName !== "" ? App.engine.rigName : qsTr("(unnamed rig)"))
            : qsTr("Edit")
    }

    // In the chain editor: whether the library has what is playing.
    Text {
        anchors.left: title.right
        anchors.leftMargin: 12
        anchors.baseline: title.baseline
        visible: bar.inEditor && !bar.editingNode && text !== ""
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
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8

        Row {
            spacing: 6
            visible: bar.inEditor && !bar.editingNode && !App.palette.isOpen
            anchors.verticalCenter: parent.verticalCenter
            HeaderButton { height: 40; label: "+"; enabled: App.rig.hasDocument && !App.palette.isOpen; onClicked: App.openPalette() }
            HeaderButton { height: 40; label: qsTr("Undo"); enabled: App.rig.canUndo; onClicked: App.rig.undo() }
            HeaderButton { height: 40; label: qsTr("Redo"); enabled: App.rig.canRedo; onClicked: App.rig.redo() }
        }

        // The engine's health, and how much of the render budget the rig takes.
        Row {
            spacing: 8
            anchors.verticalCenter: parent.verticalCenter
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 12; height: 12; radius: 6
                color: bar.connected ? bar.healthColor(App.engine.health)
                     : App.engine.connection === EngineSession.Connecting ? Theme.warn : Theme.bad
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                visible: bar.connected
                text: App.engine.dspPercent < 0 ? qsTr("DSP —") : qsTr("DSP %1 %").arg(Math.round(App.engine.dspPercent))
                color: App.engine.dspPercent >= 90 ? Theme.bad : App.engine.dspPercent >= 70 ? Theme.warn : Theme.dim
                font.pixelSize: Theme.fontSmall
            }
        }
    }

    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.line }
}
