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
// implementation; this file only places bands.
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

    // The strobe's geometry. One band period, as in the reference implementation (64 px there, and
    // the same here so the two read alike), and the rows' own heights.
    readonly property int period: 64
    readonly property int rowHeight: 30
    readonly property int rowGap: 6

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
            Column {
                id: note
                x: 28
                anchors.verticalCenter: parent.verticalCenter
                width: 280
                spacing: 0
                Text {
                    text: view.hasNote ? view.tuner.noteText : "\u2014"
                    color: view.hasNote ? (view.tuner.inTune ? Theme.good : Theme.text) : Theme.faint
                    font.pixelSize: 132
                    font.weight: Font.DemiBold
                }
                Text {
                    x: 6
                    text: view.tuner.centsText
                    color: view.tuner.inTune ? Theme.good : Theme.dim
                    font.pixelSize: Theme.fontLarge
                    visible: view.hasNote
                }
            }

            // The cents scale: half a semitone either side of the note, and the in-tune band.
            Item {
                id: scale
                x: note.x + note.width + 36
                anchors.verticalCenter: parent.verticalCenter
                width: view.tuner.barWidth
                height: 200

                readonly property real mid: width / 2
                readonly property real halfBar: width / 2
                readonly property real inTuneWidth: width * (view.tuner.inTuneCents / view.tuner.maxCents)

                // A tick every tenth of the way, taller at the centre and at the ends.
                Repeater {
                    model: 11
                    delegate: Rectangle {
                        required property int index
                        readonly property real cents: (index - 5) * (view.tuner.maxCents / 5)
                        x: scale.mid + cents / view.tuner.maxCents * scale.halfBar - width / 2
                        anchors.verticalCenter: parent.verticalCenter
                        width: index === 5 ? 3 : 1
                        height: index === 5 ? 160 : index === 0 || index === 10 ? 48 : 24
                        color: index === 5 ? Theme.dim : Theme.line
                    }
                }

                // In tune, drawn as a band rather than said in words.
                Rectangle {
                    x: scale.mid - scale.inTuneWidth / 2
                    anchors.verticalCenter: parent.verticalCenter
                    width: scale.inTuneWidth
                    height: 160
                    radius: 4
                    color: Theme.accentSoft
                }

                Rectangle {
                    visible: view.hasNote
                    x: view.tuner.needleX - width / 2
                    anchors.verticalCenter: parent.verticalCenter
                    width: 5
                    height: 190
                    radius: 2
                    color: view.tuner.inTune ? Theme.good : Theme.accent
                    Behavior on x { NumberAnimation { duration: 40 } }
                }

                Text {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    text: "\u2212" + view.tuner.maxCents
                    color: Theme.faint
                    font.pixelSize: Theme.fontSmall
                }
                Text {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    text: "+" + view.tuner.maxCents
                    color: Theme.faint
                    font.pixelSize: Theme.fontSmall
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    text: qsTr("cents")
                    color: Theme.faint
                    font.pixelSize: Theme.fontSmall
                }
            }

            // The numbers beside the picture, and the switch under them.
            Column {
                x: scale.x + scale.width + 40
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

        // The strobe: three rows, one per partial. It is drawn from bands moved by a phase rather
        // than repainted: 40 fixed bands per row, translated, wrapped by the clip.
        Item {
            id: strobe
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: view.rowHeight * 3 + view.rowGap * 2 + 20

            Text {
                id: legend
                anchors.right: parent.right
                anchors.top: parent.top
                text: qsTr("drifts left: flat   ·   still: in tune   ·   drifts right: sharp")
                color: Theme.faint
                font.pixelSize: Theme.fontSmall
            }

            Column {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                spacing: view.rowGap

                // Three rows for three partials. `model: 3` rather than an array of the phases: an
                // array built from bindings would be rebuilt every frame and take the delegates with
                // it, so the phase is a binding on each row instead.
                Repeater {
                    model: 3
                    delegate: Item {
                        id: row
                        required property int index
                        readonly property real phase: index === 0 ? view.tuner.rowPhase1
                                                     : index === 1 ? view.tuner.rowPhase2
                                                                   : view.tuner.rowPhase4
                        readonly property string label: index === 0 ? "1x" : index === 1 ? "2x" : "4x"
                        width: parent.width
                        height: view.rowHeight

                        // The row's paper, then its bands, clipped to the row.
                        Rectangle {
                            anchors.fill: parent
                            radius: 3
                            color: Theme.menuBackground
                        }

                        Item {
                            anchors.fill: parent
                            anchors.leftMargin: 44
                            clip: true

                            Item {
                                // Rightward drift for a sharp string, as the reference has it: the
                                // phase grows and the bands move with it.
                                x: row.phase * view.period - view.period
                                width: view.period * 40
                                height: parent.height
                                // One binding for the row rather than one per band: 120 opacity
                                // updates a frame is not a cost worth paying for a fade.
                                opacity: view.tuner.strobeAlpha

                                Repeater {
                                    model: 40
                                    delegate: Rectangle {
                                        required property int index
                                        x: index * view.period
                                        width: view.period - 26
                                        height: parent.height
                                        color: Theme.dim
                                    }
                                }
                            }
                        }

                        // The label chip, on its own background so it reads over the bands.
                        Rectangle {
                            width: 40
                            height: parent.height
                            radius: 3
                            color: Theme.panel
                            Text {
                                anchors.centerIn: parent
                                text: row.label
                                color: Theme.text
                                font.pixelSize: Theme.fontSmall
                                font.weight: Font.DemiBold
                            }
                        }
                    }
                }
            }
        }
    }
}
