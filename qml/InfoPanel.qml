import QtQuick
import Rigpa.Core

// One widget, full height: the engine's connection, health, load and xruns, and the meters.
// M2 fills it in; until then it says so.
Rectangle {
    color: Theme.panel

    Rectangle { width: 1; height: parent.height; color: Theme.line }

    Column {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 14

        Text { text: qsTr("Engine"); color: Theme.dim; font.pixelSize: Theme.fontSmall }
        Row {
            spacing: 10
            Rectangle { width: 12; height: 12; radius: 6; color: Theme.faint; anchors.verticalCenter: parent.verticalCenter }
            Text { text: qsTr("not connected"); color: Theme.text; font.pixelSize: Theme.fontNormal }
        }
        Text {
            width: parent.width
            wrapMode: Text.WordWrap
            text: qsTr("Health, DSP load, xruns and the level meters arrive with the engine connection (M2).")
            color: Theme.faint
            font.pixelSize: Theme.fontSmall
        }
        Item { width: 1; height: 8 }
        Text {
            text: App.settings.liveMeters ? qsTr("Meters: on") : qsTr("Meters: off")
            color: Theme.dim
            font.pixelSize: Theme.fontSmall
        }
    }
}
