import QtQuick
import Rigpa
import Rigpa.Core

// The library's rigs, by name, with the running one marked. The rotary walks them when the list
// has the hardware focus (back from the editor); apply or a tap loads one.
Panel {
    id: panel
    readonly property FocusRing ring: App.nav.rigList
    clip: true

    function sync() { ring.count = App.rigs.count }
    Component.onCompleted: sync()
    Connections {
        target: App.rigs
        function onChanged() { panel.sync() }
    }
    // Arriving in the list starts on the rig that's running.
    onFocusedChanged: if (focused && App.rigs.runningIndex >= 0) ring.highlight = App.rigs.runningIndex
    Connections {
        target: panel.ring
        function onHighlightChanged() { list.positionViewAtIndex(panel.ring.highlight, ListView.Contain) }
    }

    ListView {
        id: list
        anchors.fill: parent
        anchors.margins: 6
        model: App.rigs
        boundsBehavior: Flickable.StopAtBounds
        delegate: Rectangle {
            id: row
            required property int index
            required property string name
            required property bool running
            readonly property bool highlighted: panel.focused && panel.ring.highlight === index
            width: ListView.view.width
            height: 52
            radius: 8
            color: tap.pressed ? Theme.raised : running ? Theme.accentSoft : "transparent"
            border.width: highlighted ? Theme.highlightWidth : 0
            border.color: Theme.accent
            Rectangle {
                x: 12
                anchors.verticalCenter: parent.verticalCenter
                width: 10; height: 10; radius: 5
                color: row.running ? Theme.accent : "transparent"
                border.color: row.running ? Theme.accent : Theme.faint
            }
            Text {
                x: 32
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - 44
                elide: Text.ElideRight
                text: row.name
                color: Theme.text
                font.pixelSize: Theme.fontNormal
                font.weight: row.running ? Font.DemiBold : Font.Normal
            }
            TapHandler {
                id: tap
                onTapped: {
                    panel.ring.highlight = row.index
                    App.rig.loadRig(row.index)
                    App.nav.focusRigEditor()
                }
            }
        }
    }

    Text {
        anchors.centerIn: parent
        width: parent.width - 32
        visible: list.count === 0
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        text: App.rigs.problem !== "" ? App.rigs.problem : App.rigs.loaded ? qsTr("No rigs in the library") : qsTr("Asking the pedal for its rigs…")
        color: Theme.dim
        font.pixelSize: Theme.fontSmall
    }
}
