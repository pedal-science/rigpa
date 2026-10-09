import QtQuick
import Rigpa
import Rigpa.Core

// The models and IRs on the pedal, with their metadata (M3). The Models/IRs switch is the first
// stop for the rotary; the list's items follow it.
Item {
    id: view
    property int kind: 0   // 0 models, 1 IRs
    readonly property FocusRing ring: App.nav.browse
    readonly property bool active: App.nav.mainView === Navigator.BrowseView

    Component.onCompleted: { ring.kinds = ["toggle"]; ring.count = 1 }

    Connections {
        target: view.ring
        function onToggled(index) { if (index === 0) view.kind = 1 - view.kind }
    }

    Segmented {
        x: 24; y: 20
        labels: [qsTr("Models"), qsTr("IRs")]
        current: view.kind
        highlighted: view.active && view.ring.highlight === 0
        onChosen: (index) => { view.ring.highlight = 0; view.kind = index }
    }

    Panel {
        x: 24; y: 80
        width: parent.width - 48
        height: parent.height - 100
        Text {
            anchors.centerIn: parent
            text: view.kind === 0 ? qsTr("NAM models arrive in M3") : qsTr("IRs arrive in M3")
            color: Theme.dim
            font.pixelSize: Theme.fontNormal
        }
    }
}
