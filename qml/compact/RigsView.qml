import QtQuick
import Rigpa
import Rigpa.Core

// The compact layout's rig list, full screen: the library's rigs by name with the running one
// marked, and New, Rename (or Save as…), Copy and Delete under them.
//
// The rotary walks the rigs and then the four buttons. Apply on a rig loads it and goes home to
// Play (Navigator does that); apply on a button opens its dialog, as a tap does.
Item {
    id: view
    readonly property FocusRing ring: App.nav.rigList
    readonly property bool active: App.nav.mainView === Navigator.RigsView && App.nav.overlay === Navigator.NoOverlay
    // A rig the library doesn't have (one ljoss applied) can only be saved.
    readonly property bool inLibrary: App.rigs.running !== ""
    readonly property var actions: [
        { label: qsTr("New"), enabled: true },
        { label: view.inLibrary ? qsTr("Rename") : qsTr("Save as…"), enabled: view.inLibrary || App.rig.hasDocument },
        { label: qsTr("Copy"), enabled: view.inLibrary },
        { label: qsTr("Delete"), enabled: view.inLibrary }
    ]

    // The ring's stops: one per rig, then one per button.
    function sync() {
        const kinds = []
        for (let i = 0; i < App.rigs.count; ++i)
            kinds.push("item")
        for (let j = 0; j < actions.length; ++j)
            kinds.push("action")
        ring.kinds = kinds
        ring.count = kinds.length
    }
    function act(button) {
        if (button < 0 || button >= actions.length || !actions[button].enabled)
            return
        switch (button) {
        case 0: App.manager.begin("new"); break
        case 1: App.manager.begin(view.inLibrary ? "rename" : "saveAs"); break
        case 2: App.manager.begin("duplicate"); break
        case 3: App.manager.begin("delete"); break
        }
    }
    function load(index) {
        App.rig.loadRig(index)
        App.nav.showMainView(Navigator.PlayView)
    }

    Component.onCompleted: sync()
    Connections {
        target: App.rigs
        function onChanged() { view.sync() }
    }
    // Arriving in the list starts on the rig that's running, and while the list is up the highlight
    // follows it (a footswitch moved it, or the list arrived after the view did).
    function showRunning() {
        if (visible && App.rigs.runningIndex >= 0)
            ring.highlight = App.rigs.runningIndex
    }
    onVisibleChanged: showRunning()
    Connections {
        target: App.rigs
        function onRunningChanged() { view.showRunning() }
    }
    Connections {
        target: view.ring
        function onActivated(index) { view.act(index - App.rigs.count) }
        function onHighlightChanged() {
            if (view.ring.highlight < App.rigs.count)
                list.positionViewAtIndex(view.ring.highlight, ListView.Contain)
        }
    }

    ListView {
        id: list
        x: 16; y: 10
        width: parent.width - 32
        height: actionRow.y - y - 10
        clip: true
        model: App.rigs
        boundsBehavior: Flickable.StopAtBounds
        spacing: 4
        delegate: Rectangle {
            id: row
            required property int index
            required property string name
            required property bool running
            readonly property bool highlighted: view.active && view.ring.highlight === index
            width: ListView.view.width
            height: 56
            radius: 8
            color: tap.pressed ? Theme.raised : running ? Theme.accentSoft : Theme.panel
            border.width: highlighted ? Theme.highlightWidth : 0
            border.color: Theme.accent
            Rectangle {
                x: 16
                anchors.verticalCenter: parent.verticalCenter
                width: 12; height: 12; radius: 6
                color: row.running ? Theme.accent : "transparent"
                border.color: row.running ? Theme.accent : Theme.faint
            }
            Text {
                x: 40
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - 56
                elide: Text.ElideRight
                text: row.name
                color: Theme.text
                font.pixelSize: Theme.fontLarge
                font.weight: row.running ? Font.DemiBold : Font.Normal
            }
            TapHandler {
                id: tap
                onTapped: {
                    view.ring.highlight = row.index
                    view.load(row.index)
                }
            }
        }
    }

    Text {
        anchors.centerIn: list
        width: list.width - 32
        visible: list.count === 0
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        text: App.rigs.problem !== "" ? App.rigs.problem : App.rigs.loaded ? qsTr("No rigs in the library") : qsTr("Asking the pedal for its rigs…")
        color: Theme.dim
        font.pixelSize: Theme.fontNormal
    }

    Row {
        id: actionRow
        x: 16
        y: parent.height - height - 12
        spacing: 10
        Repeater {
            model: view.actions
            delegate: HeaderButton {
                required property int index
                required property var modelData
                width: (view.width - 32 - 30) / 4
                height: 56
                base: Theme.raised
                label: modelData.label
                enabled: modelData.enabled
                highlighted: view.active && view.ring.highlight === App.rigs.count + index
                onClicked: {
                    view.ring.highlight = App.rigs.count + index
                    view.act(index)
                }
            }
        }
    }
}
