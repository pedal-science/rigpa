import QtQuick
import Rigpa
import Rigpa.Core

// The tuner, as a main view: the note, how far off it is, a strobe, and the switch.
//
// While this is the view, rigpa is running a tuner node after each chain's input and that node's
// output is silence, so the rig is muted for exactly as long as this is on screen (App's
// mainViewChanged connection, and namchain docs/tuner.md). The switch at the bottom right opens it:
// the input then passes through unchanged and the display carries on, which is how a player tunes
// with the rig audible. Back returns to the rig view, which removes the node again.
//
// The strobe is the part that makes it a strobe tuner: three rows for partials 1, 2 and 4 of the
// note, labelled 1x, 2x and 4x. Row k drifts k times as fast as the fundamental, so the bottom row
// shows an error the top one barely shows, and in tune every row stands still. The law lives in
// TunerModel (kDriftPerCent, kMaxDrift) because it came from squatch-tuner's reference
// implementation; TunerStrobe only places bands. The note, the scale and the strobe are shared with
// the compact layout's tuner (CompactTunerView), which arranges them for the 800×480 panel.
//
// It has one stop for the rotary -- the switch -- so a pedal with no keyboard can still open it.
// Everything else here reports; apply on the switch toggles it.
Item {
    id: view
    readonly property var tuner: App.tuner
    readonly property FocusRing ring: App.nav.tuner
    readonly property bool active: App.nav.mainView === Navigator.TunerView
    readonly property bool hasNote: tuner.valid && tuner.noteText !== ""
    // The player's intent, not the engine's report: the button must move when it is pressed, and
    // RigSession puts it back if the engine refuses the patch.
    readonly property bool muted: App.rig.tunerMuted

    Component.onCompleted: {
        ring.kinds = ["toggle"];
        ring.count = 1;
    }

    Connections {
        target: view.ring
        function onToggled(index) { App.rig.setTunerMuted(!App.rig.tunerMuted) }
    }

    // The strobe advances on its own clock rather than on the reading's: 60 frames a second is what
    // makes a drift smooth, and reading arrives at 30. `advance` decides whether there is anything
    // to drift.
    Timer {
        interval: 16
        repeat: true
        running: view.active
        onTriggered: view.tuner.advance(interval / 1000)
    }

    Panel {
        anchors.fill: parent
        anchors.margins: 12

        // The picture: the note, the cents scale, and the numbers beside them.
        Item {
            id: top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: parent.height - strobe.height - 10

            // The note: what the player is looking for, big enough to read from standing.
            TunerNote {
                id: note
                x: 28
                anchors.verticalCenter: parent.verticalCenter
                width: 280
                tuner: view.tuner
                hasNote: view.hasNote
                noteSize: 132
            }

            // The cents scale: half a semitone either side of the note, and the in-tune band.
            TunerScale {
                id: meter
                x: note.x + note.width + 36
                anchors.verticalCenter: parent.verticalCenter
                height: 200
                tuner: view.tuner
                hasNote: view.hasNote
            }

            // The numbers beside the picture, and the switch under them.
            Column {
                x: meter.x + meter.width + 40
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - x - 28
                spacing: 10

                Text {
                    text: view.hasNote ? view.tuner.hzText : ""
                    color: Theme.text
                    font.pixelSize: Theme.fontLarge
                    visible: text !== ""
                }

                // The input's level, from -60 dBFS up: whether anything is arriving at all.
                Item {
                    width: Math.min(parent.width, 380)
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
                Row {
                    spacing: 14
                    Text {
                        text: view.tuner.levelText
                        color: Theme.dim
                        font.pixelSize: Theme.fontSmall
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        text: view.tuner.a4Text
                        color: Theme.faint
                        font.pixelSize: Theme.fontSmall
                        anchors.verticalCenter: parent.verticalCenter
                        visible: text !== ""
                    }
                }

                // The switch. A toggle rather than a button because it has a state, and the state is
                // the engine's: the label says what the rig is doing, not what was asked for.
                Row {
                    spacing: 14
                    Toggle {
                        anchors.verticalCenter: parent.verticalCenter
                        checked: view.muted
                        highlighted: view.active && view.ring.highlight === 0
                        onToggled: App.rig.setTunerMuted(!App.rig.tunerMuted)
                    }
                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 2
                        Text {
                            text: view.muted ? qsTr("Muted") : qsTr("Audible")
                            color: view.muted ? Theme.dim : Theme.warn
                            font.pixelSize: Theme.fontNormal
                        }
                    }
                }

                Text {
                    width: parent.width
                    text: view.tuner.status
                    color: Theme.warn
                    font.pixelSize: Theme.fontNormal
                    wrapMode: Text.WordWrap
                    visible: text !== "" && !view.tuner.running
                }
            }
        }

        // The strobe: three rows, one per partial (TunerStrobe).
        TunerStrobe {
            id: strobe
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            tuner: view.tuner
        }
    }
}
