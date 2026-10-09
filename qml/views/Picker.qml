import QtQuick
import Rigpa
import Rigpa.Core

// Choosing a model or an IR for the node editor: the library as a list, grouped by folder, opened
// on what the block holds now. The rotary moves through it, apply or a tap chooses, back closes.
Rectangle {
    id: picker
    readonly property var model: App.picker
    readonly property FocusRing ring: model.ring
    readonly property bool active: App.nav.overlay === Navigator.Picker
    color: Theme.background

    Connections {
        target: picker.model
        function onOpenChanged() {
            scroll.stop()
            if (picker.model.isOpen)
                Qt.callLater(() => list.show(picker.ring.highlight, false))
        }
    }
    Connections {
        target: picker.ring
        // While it is opening, the open handler places the list; after that, follow the rotary.
        function onHighlightChanged() { if (picker.visible) list.show(picker.ring.highlight, true) }
    }

    Item {
        id: titleRow
        x: 24; y: 10
        width: parent.width - 48
        height: 48
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: picker.model.kind === 0 ? qsTr("Choose a model") : qsTr("Choose an IR")
            color: Theme.text
            font.pixelSize: Theme.fontLarge
        }
        HeaderButton {
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
            model: picker.model
            boundsBehavior: Flickable.StopAtBounds
            highlightFollowsCurrentItem: false

            function show(index, animate) {
                if (index < 0 || index >= count)
                    return
                const before = contentY
                positionViewAtIndex(index, animate ? ListView.Contain : ListView.Center)
                const after = contentY
                if (animate && Math.abs(after - before) > 1) {
                    scroll.from = before
                    scroll.to = after
                    scroll.restart()
                }
            }
            NumberAnimation { id: scroll; target: list; property: "contentY"; duration: 140; easing.type: Easing.OutCubic }

            section.property: "group"
            section.criteria: ViewSection.FullString
            section.delegate: Item {
                required property string section
                width: ListView.view.width
                height: 34
                Text {
                    x: 14
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 6
                    width: parent.width - 28
                    elide: Text.ElideMiddle
                    text: parent.section === "" ? qsTr("Top level") : parent.section
                    color: Theme.dim
                    font.pixelSize: Theme.fontSmall
                    font.weight: Font.DemiBold
                }
            }

            delegate: Rectangle {
                id: row
                required property int index
                required property string name
                required property string detail
                required property bool isCurrent
                readonly property bool highlighted: picker.active && picker.ring.highlight === index

                width: ListView.view.width
                height: 56
                radius: 8
                color: tap.pressed ? Theme.raised : isCurrent ? Theme.accentSoft : "transparent"
                border.width: highlighted ? Theme.highlightWidth : 0
                border.color: Theme.accent

                Column {
                    x: 14
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - 28
                    spacing: 2
                    Text {
                        width: parent.width
                        elide: Text.ElideRight
                        text: row.name
                        color: Theme.text
                        font.pixelSize: Theme.fontNormal
                    }
                    Text {
                        width: parent.width
                        elide: Text.ElideRight
                        text: row.detail
                        color: Theme.dim
                        font.pixelSize: Theme.fontSmall
                    }
                }
                TapHandler {
                    id: tap
                    onTapped: { picker.ring.highlight = row.index; picker.model.choose(row.index) }
                }
            }
        }
    }
}
