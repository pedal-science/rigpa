import QtQuick
import Rigpa
import Rigpa.Core

// The models and IRs on the pedal, with what their files say about them (read directly and
// read-only; namchaind doesn't read NAM metadata). The Models/IRs switch is the first stop for
// the rotary; the list's rows follow it, and the list keeps the highlighted row in view.
Item {
    id: view
    readonly property FocusRing ring: App.nav.browse
    readonly property bool active: App.nav.mainView === Navigator.BrowseView
    readonly property var library: App.library

    // Ring index 0 is the switch; row i of the list is ring index i + 1.
    function syncRing() {
        ring.kinds = ["toggle"]
        ring.count = 1 + list.count
    }
    Component.onCompleted: syncRing()
    onActiveChanged: if (active) library.rescan()

    Connections {
        target: view.library
        function onModelReset() { view.syncRing() }
    }
    Connections {
        target: view.ring
        function onToggled(index) { if (index === 0) view.library.kind = 1 - view.library.kind }
        function onHighlightChanged() {
            if (view.ring.highlight > 0)
                list.currentIndex = view.ring.highlight - 1
        }
    }

    Segmented {
        id: kinds
        x: 24; y: 18
        labels: [qsTr("Models"), qsTr("IRs")]
        current: view.library.kind
        highlighted: view.active && view.ring.highlight === 0
        onChosen: (index) => { view.ring.highlight = 0; view.library.kind = index }
    }

    Text {
        anchors.left: kinds.right
        anchors.leftMargin: 24
        anchors.verticalCenter: kinds.verticalCenter
        color: Theme.dim
        font.pixelSize: Theme.fontSmall
        text: view.library.scanning && view.library.modelCount + view.library.irCount === 0 ? qsTr("Reading the library…")
            : view.library.problem !== "" ? view.library.problem
            : qsTr("%1 models · %2 IRs").arg(view.library.modelCount).arg(view.library.irCount)
    }

    Panel {
        x: 24; y: 76
        width: parent.width - 48
        height: parent.height - 88
        clip: true

        ListView {
            id: list
            anchors.fill: parent
            anchors.margins: 6
            model: view.library
            boundsBehavior: Flickable.StopAtBounds
            currentIndex: -1
            highlightFollowsCurrentItem: false
            // The rotary moves the highlight; the list scrolls only as far as it must to keep it
            // in view, with a short animation so the jump is followable.
            onCurrentIndexChanged: if (currentIndex >= 0) scrollTo(currentIndex)
            function scrollTo(index) {
                const before = contentY
                positionViewAtIndex(index, ListView.Contain)
                const after = contentY
                if (Math.abs(after - before) > 1) {
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
                height: 36
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
                readonly property bool highlighted: view.active && view.ring.highlight === index + 1

                width: ListView.view.width
                height: 58
                radius: 8
                color: tap.pressed ? Theme.raised : "transparent"
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
                    onTapped: view.ring.highlight = row.index + 1
                }
            }

            Text {
                anchors.centerIn: parent
                visible: list.count === 0 && !view.library.scanning
                text: view.library.kind === 0 ? qsTr("No models") : qsTr("No IRs")
                color: Theme.dim
                font.pixelSize: Theme.fontNormal
            }
        }
    }
}
