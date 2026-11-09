import QtQuick
import Rigpa
import Rigpa.Core

// The pedal's banks, in the order the footswitches walk them. A rig is picked up with apply or a tap,
// moved with the rotary or its arrows (past the end of its bank, into the next), or dragged where it
// goes; putting it down saves the banks on the pedal. Back puts a lifted rig down, then closes.
Rectangle {
    id: page
    readonly property var model: App.banks
    readonly property FocusRing ring: model.ring
    readonly property bool active: App.nav.overlay === Navigator.Banks
    readonly property real rowHeight: 52
    color: Theme.background

    Connections {
        target: App.nav
        function onOverlayChanged() { if (App.nav.overlay !== Navigator.Banks && page.model.isOpen) page.model.close() }
    }
    Connections {
        target: page.ring
        function onHighlightChanged() { if (page.visible) list.positionViewAtIndex(page.ring.highlight, ListView.Contain) }
    }

    Item {
        id: titleRow
        x: 24; y: 10
        width: parent.width - 48
        height: 48
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Banks")
            color: Theme.text
            font.pixelSize: Theme.fontLarge
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: close.left
            anchors.rightMargin: 16
            width: parent.width - 200
            horizontalAlignment: Text.AlignRight
            elide: Text.ElideRight
            text: page.model.problem !== "" ? page.model.problem
                  : page.model.lifted >= 0 ? qsTr("Move it, then put it down to save")
                  : qsTr("Pick a rig up to move it")
            color: page.model.problem !== "" ? Theme.bad : Theme.dim
            font.pixelSize: Theme.fontSmall
        }
        HeaderButton {
            id: close
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            label: qsTr("Close")
            onClicked: App.nav.closeOverlay()
        }
    }

    Panel {
        x: 24
        y: titleRow.y + titleRow.height + 8
        width: parent.width - 48
        height: parent.height - y - 10
        clip: true

        ListView {
            id: list
            anchors.fill: parent
            anchors.margins: 6
            model: page.model
            boundsBehavior: Flickable.StopAtBounds
            interactive: page.model.lifted < 0
            section.property: "bank"
            section.delegate: Text {
                required property string section
                width: ListView.view.width
                height: 34
                verticalAlignment: Text.AlignBottom
                leftPadding: 8
                bottomPadding: 4
                text: section
                color: Theme.dim
                font.pixelSize: Theme.fontSmall
                font.weight: Font.DemiBold
            }
            delegate: Rectangle {
                id: row
                required property int index
                required property string name
                required property bool lifted
                required property bool running
                readonly property bool highlighted: page.active && page.ring.highlight === index
                width: ListView.view.width
                height: page.rowHeight
                radius: 8
                color: lifted ? Theme.accentSoft : tap.pressed ? Theme.raised : "transparent"
                border.width: highlighted || lifted ? Theme.highlightWidth : 0
                border.color: Theme.accent
                z: drag.active ? 2 : 1
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
                    width: parent.width - 44 - (row.lifted ? 130 : 0)
                    elide: Text.ElideRight
                    text: row.name
                    color: Theme.text
                    font.pixelSize: Theme.fontNormal
                    font.weight: row.lifted ? Font.DemiBold : Font.Normal
                }
                Row {
                    anchors.right: parent.right
                    anchors.rightMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 6
                    visible: row.lifted
                    HeaderButton { width: 60; height: 40; label: "▲"; onClicked: page.model.move(-1) }
                    HeaderButton { width: 60; height: 40; label: "▼"; onClicked: page.model.move(1) }
                }
                TapHandler {
                    id: tap
                    onTapped: {
                        if (row.lifted) {
                            page.model.drop()
                        } else {
                            page.ring.highlight = row.index
                            page.model.lift(row.index)
                        }
                    }
                }
                // A drag: hold, move, let go where it goes. Rows are the same height, so where it is
                // let go says which row it lands on.
                DragHandler {
                    id: drag
                    target: null
                    yAxis.enabled: true
                    xAxis.enabled: false
                    onActiveChanged: {
                        if (active)
                            return
                        const to = list.indexAt(10, row.mapToItem(list.contentItem, 0, centroid.position.y).y)
                        if (to >= 0 && to !== row.index)
                            page.model.moveTo(row.index, to)
                    }
                }
            }
        }

        Text {
            anchors.centerIn: parent
            visible: list.count === 0
            text: qsTr("The pedal hasn't said what its banks are")
            color: Theme.dim
            font.pixelSize: Theme.fontNormal
        }
    }
}
