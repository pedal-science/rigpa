import QtQuick
import Rigpa
import Rigpa.Core

// The compact layout's home: what is playing, big enough to read from standing over a floor
// pedal, where the footswitches are, what changed last, and the rig's blocks, each one a switch.
//
// The rotary walks the blocks and then the four buttons: apply on a block switches it on or off,
// on a button opens its view (Navigator, the compact rows of the hardware table).
Item {
    id: view
    readonly property FocusRing ring: App.nav.play
    readonly property bool active: App.nav.mainView === Navigator.PlayView && App.nav.overlay === Navigator.NoOverlay
    readonly property var play: App.play
    readonly property var pedal: App.pedal
    readonly property var buttons: [qsTr("Rigs"), qsTr("Edit"), qsTr("Tuner"), qsTr("Settings")]

    // The ring's stops: one per block (a switch), then one per button (an action).
    function sync() {
        const kinds = []
        for (let i = 0; i < play.count; ++i)
            kinds.push("toggle")
        for (let j = 0; j < buttons.length; ++j)
            kinds.push("action")
        ring.kinds = kinds
        ring.count = kinds.length
    }
    Component.onCompleted: sync()
    Connections {
        target: App.play
        function onChanged() { view.sync() }
    }
    Connections {
        target: view.ring
        function onToggled(index) { if (index < view.play.count) view.play.toggle(index) }
        function onActivated(index) { view.open(index - view.play.count) }
        function onHighlightChanged() {
            if (view.ring.highlight < view.play.count)
                chips.positionViewAtIndex(view.ring.highlight, GridView.Contain)
        }
    }

    function open(button) {
        switch (button) {
        case 0: App.nav.showMainView(Navigator.RigsView); break
        case 1: App.nav.showMainView(Navigator.RigView); break
        case 2: if (App.engine.hasTuner) App.nav.showMainView(Navigator.TunerView); break
        case 3: App.nav.showMainView(Navigator.SettingsView); break
        }
    }

    // The rig's name, as large as the panel allows.
    Text {
        id: name
        x: 24; y: 6
        width: parent.width - 48
        elide: Text.ElideRight
        font.pixelSize: App.layout.playTitleFont
        font.weight: Font.DemiBold
        color: App.engine.hasRig ? Theme.text : Theme.dim
        text: App.engine.connection !== EngineSession.Connected ? "—"
            : App.engine.hasRig ? (App.engine.rigName !== "" ? App.engine.rigName : qsTr("(unnamed rig)"))
            : qsTr("No rig")
    }

    // Where the footswitches are, and the rig the next press reaches (pedald's word).
    Text {
        id: where
        x: 26
        anchors.top: name.bottom
        width: parent.width - 52
        elide: Text.ElideRight
        font.pixelSize: Theme.fontLarge
        color: Theme.dim
        text: {
            const p = view.pedal
            if (!p.reachable)
                return qsTr("The pedal service isn't answering")
            const parts = []
            if (p.bank >= 0 && p.position >= 0 && p.size > 0)
                parts.push(qsTr("Bank %1 · %2 of %3").arg(p.bank + 1).arg(p.position + 1).arg(p.size))
            else if (p.bank >= 0)
                parts.push(qsTr("Bank %1 · not in it").arg(p.bank + 1))
            if (p.next !== "")
                parts.push(qsTr("Next: %1").arg(p.next))
            return parts.join("   ·   ")
        }
    }

    // What changed last.
    Text {
        id: last
        x: 26
        anchors.top: where.bottom
        anchors.topMargin: 4
        width: parent.width - 52
        elide: Text.ElideRight
        font.pixelSize: Theme.fontNormal
        color: Theme.accent
        text: view.play.lastChange
    }

    // The blocks, in signal order: a tap switches one on or off.
    GridView {
        id: chips
        x: 16
        y: 150
        width: parent.width - 32
        height: buttonRow.y - y - 10
        cellWidth: Math.floor(width / 4)
        cellHeight: 72
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: App.play
        delegate: Item {
            id: cell
            required property int index
            required property string title
            required property string subtitle
            required property bool bypassed
            required property bool missing
            readonly property bool highlighted: view.active && view.ring.highlight === index
            width: chips.cellWidth
            height: chips.cellHeight

            Rectangle {
                anchors.fill: parent
                anchors.margins: 5
                radius: Theme.radius
                color: tap.pressed ? Theme.raised : cell.bypassed ? Theme.panel : Theme.accentSoft
                border.width: cell.highlighted ? Theme.highlightWidth : 1
                border.color: cell.highlighted ? Theme.accent : cell.bypassed ? Theme.line : Qt.darker(Theme.accent, 1.4)

                // The lamp: lit while the block is on.
                Rectangle {
                    x: 12
                    anchors.verticalCenter: parent.verticalCenter
                    width: 14; height: 14; radius: 7
                    color: cell.bypassed ? "transparent" : Theme.accent
                    border.width: 2
                    border.color: cell.bypassed ? Theme.faint : Theme.accent
                }
                Column {
                    x: 36
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - 44
                    Text {
                        width: parent.width
                        elide: Text.ElideRight
                        text: cell.title
                        color: cell.missing ? Theme.warn : cell.bypassed ? Theme.dim : Theme.text
                        font.pixelSize: Theme.fontNormal
                        font.weight: Font.DemiBold
                    }
                    Text {
                        width: parent.width
                        elide: Text.ElideRight
                        text: cell.missing ? qsTr("file missing") : cell.bypassed ? qsTr("off") : cell.subtitle
                        color: Theme.dim
                        font.pixelSize: Theme.fontSmall
                    }
                }
                TapHandler {
                    id: tap
                    onTapped: {
                        view.ring.highlight = cell.index
                        view.play.toggle(cell.index)
                    }
                }
            }
        }
    }

    Text {
        anchors.centerIn: chips
        visible: App.engine.hasRig && view.play.count === 0
        text: qsTr("This rig has no blocks")
        color: Theme.dim
        font.pixelSize: Theme.fontNormal
    }

    // The way to the other views.
    Row {
        id: buttonRow
        x: 16
        y: parent.height - height - 12
        spacing: 10
        Repeater {
            model: view.buttons
            delegate: HeaderButton {
                required property int index
                required property string modelData
                width: (view.width - 32 - 30) / 4
                height: 56
                base: Theme.raised
                label: modelData
                enabled: index !== 2 || App.engine.hasTuner
                highlighted: view.active && view.ring.highlight === view.play.count + index
                onClicked: {
                    view.ring.highlight = view.play.count + index
                    view.open(index)
                }
            }
        }
    }
}
