import QtQuick
import Rigpa
import Rigpa.Core

// The app's settings. Every row is a stop for the rotary, in reading order.
Item {
    id: view
    readonly property FocusRing ring: App.nav.settings
    readonly property bool active: App.nav.mainView === Navigator.SettingsView

    Component.onCompleted: { ring.kinds = ["value", "toggle"]; ring.count = 2 }

    Connections {
        target: view.ring
        function onStepped(index, delta) { if (index === 0) App.settings.stepRigListWidth(delta) }
        function onToggled(index) { if (index === 1) App.settings.liveMeters = !App.settings.liveMeters }
    }

    Column {
        x: 40; y: 32
        width: 1000
        spacing: 28

        Row {
            spacing: 24
            Text {
                width: 260
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Rig list width")
                color: Theme.text
                font.pixelSize: Theme.fontLarge
            }
            ValueBar {
                width: 700
                from: App.settings.rigListWidthMin
                to: App.settings.rigListWidthMax
                value: App.settings.rigListWidth
                text: App.settings.rigListWidth + " px"
                highlighted: view.active && view.ring.highlight === 0
                editing: highlighted && view.ring.editing
                onPressed: view.ring.highlight = 0
                onMoved: (v) => App.settings.rigListWidth = Math.round(v)
            }
        }

        Row {
            spacing: 24
            Text {
                width: 260
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Live meters")
                color: Theme.text
                font.pixelSize: Theme.fontLarge
            }
            Toggle {
                checked: App.settings.liveMeters
                highlighted: view.active && view.ring.highlight === 1
                onToggled: { view.ring.highlight = 1; App.settings.liveMeters = !App.settings.liveMeters }
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Off keeps the screen still, which costs the pedal least.")
                color: Theme.dim
                font.pixelSize: Theme.fontSmall
            }
        }
    }
}
