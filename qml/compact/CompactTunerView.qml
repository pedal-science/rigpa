import QtQuick
import Rigpa
import Rigpa.Core

// The tuner on the 800×480 panel: the bar's note, scale and strobe (TunerNote, TunerScale,
// TunerStrobe), arranged for a narrower screen. The note and the scale share the top, the Hz, the
// input's level and the switch make one row under them, and the strobe runs along the bottom.
//
// Everything TunerView says about the node holds here: while this is the view the engine runs a
// tuner node after each chain's input and the rig is silent until the switch opens it, and back
// (to Play) removes the node again. Its one rotary stop is the switch.
Item {
    id: view
    readonly property var tuner: App.tuner
    readonly property FocusRing ring: App.nav.tuner
    readonly property bool active: App.nav.mainView === Navigator.TunerView
    readonly property bool hasNote: tuner.valid && tuner.noteText !== ""
    // The player's intent, not the engine's report (see TunerView).
    readonly property bool muted: App.rig.tunerMuted

    Component.onCompleted: {
        ring.kinds = ["toggle"];
        ring.count = 1;
    }

    Connections {
        target: view.ring
        function onToggled(index) { App.rig.setTunerMuted(!App.rig.tunerMuted) }
    }

    // The strobe's own clock, as in TunerView.
    Timer {
        interval: 16
        repeat: true
        running: view.active
        onTriggered: view.tuner.advance(interval / 1000)
    }

    Panel {
        anchors.fill: parent
        anchors.margins: 8

        TunerNote {
            id: note
            x: 16; y: 6
            width: App.layout.tunerNoteWidth
            noteSize: App.layout.tunerNoteFont
            tuner: view.tuner
            hasNote: view.hasNote
        }

        TunerScale {
            id: meter
            x: parent.width - width - 16
            y: 10
            height: App.layout.tunerScaleHeight
            tuner: view.tuner
            hasNote: view.hasNote
        }

        // Hz, whether anything is arriving at all (from -60 dBFS up), and the switch.
        Item {
            id: readout
            x: 16
            y: meter.y + meter.height + 8
            width: parent.width - 32
            height: 48

            Row {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 16
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 130
                    text: view.hasNote ? view.tuner.hzText : ""
                    color: Theme.text
                    font.pixelSize: Theme.fontLarge
                }
                Item {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 180
                    height: 16
                    Rectangle {
                        anchors.fill: parent
                        radius: 4
                        color: Theme.raised
                    }
                    Rectangle {
                        width: parent.width * view.tuner.levelFraction
                        height: parent.height
                        radius: 4
                        color: view.tuner.levelFraction > 0.95 ? Theme.bad
                             : view.tuner.levelFraction > 0.8 ? Theme.warn
                             : Theme.good
                    }
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: view.tuner.levelText
                    color: Theme.dim
                    font.pixelSize: Theme.fontSmall
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: view.tuner.a4Text
                    color: Theme.faint
                    font.pixelSize: Theme.fontSmall
                    visible: text !== ""
                }
            }

            // The switch: a toggle, because it has a state, and the label says what the rig is doing.
            Row {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: 12
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: view.muted ? qsTr("Muted") : qsTr("Audible")
                    color: view.muted ? Theme.dim : Theme.warn
                    font.pixelSize: Theme.fontNormal
                }
                Toggle {
                    anchors.verticalCenter: parent.verticalCenter
                    checked: view.muted
                    highlighted: view.active && view.ring.highlight === 0
                    onToggled: App.rig.setTunerMuted(!App.rig.tunerMuted)
                }
            }
        }

        Text {
            x: 16
            anchors.top: readout.bottom
            width: parent.width - 32
            elide: Text.ElideRight
            text: view.tuner.status
            color: Theme.warn
            font.pixelSize: Theme.fontNormal
            visible: text !== "" && !view.tuner.running
        }

        TunerStrobe {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 10
            rowHeight: 26
            tuner: view.tuner
        }
    }
}
