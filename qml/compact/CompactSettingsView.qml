import QtQuick
import Rigpa
import Rigpa.Core

// The compact layout's settings, with the engine's status beside them: the panel has no room for
// the bar's always-there info column, so it lives here (InfoPanel, at the panel's height). The rig
// list width is the bar's setting and doesn't appear; live meters is the one stop for the rotary.
Item {
    id: view
    readonly property FocusRing ring: App.nav.settings
    readonly property bool active: App.nav.mainView === Navigator.SettingsView

    Component.onCompleted: { ring.kinds = ["toggle"]; ring.count = 1 }

    Connections {
        target: view.ring
        function onToggled(index) { App.settings.liveMeters = !App.settings.liveMeters }
    }

    Column {
        x: 28; y: 28
        width: parent.width - info.width - 56
        spacing: 16

        Row {
            spacing: 24
            Text {
                width: 220
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Live meters")
                color: Theme.text
                font.pixelSize: Theme.fontLarge
            }
            Toggle {
                checked: App.settings.liveMeters
                highlighted: view.active && view.ring.highlight === 0
                onToggled: { view.ring.highlight = 0; App.settings.liveMeters = !App.settings.liveMeters }
            }
        }
        Text {
            width: parent.width
            wrapMode: Text.WordWrap
            text: qsTr("Off keeps the screen still, which costs the pedal least. The meters are in the status column.")
            color: Theme.dim
            font.pixelSize: Theme.fontSmall
        }
    }

    InfoPanel {
        id: info
        anchors.right: parent.right
        width: 320
        height: parent.height
    }
}
