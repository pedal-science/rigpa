import QtQuick
import Rigpa

// The strobe: three rows for partials 1, 2 and 4 of the note, labelled 1x, 2x and 4x. Row k drifts
// k times as fast as the fundamental, so the bottom row shows an error the top one barely shows,
// and in tune every row stands still. The law lives in TunerModel (kDriftPerCent, kMaxDrift),
// because it came from squatch-tuner's reference implementation; this only places bands. It is
// drawn from bands moved by a phase rather than repainted: 40 fixed bands per row, translated,
// wrapped by the clip. Shared by both layouts' tuner views.
Item {
    id: strobe
    required property var tuner
    // One band period, as in the reference implementation (64 px there, and the same here so the
    // two read alike), and the rows' own heights.
    property int period: 64
    property int rowHeight: 30
    property int rowGap: 6
    height: rowHeight * 3 + rowGap * 2 + 20

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
        spacing: strobe.rowGap

        // Three rows for three partials. `model: 3` rather than an array of the phases: an
        // array built from bindings would be rebuilt every frame and take the delegates with
        // it, so the phase is a binding on each row instead.
        Repeater {
            model: 3
            delegate: Item {
                id: row
                required property int index
                readonly property real phase: index === 0 ? strobe.tuner.rowPhase1
                                             : index === 1 ? strobe.tuner.rowPhase2
                                                           : strobe.tuner.rowPhase4
                readonly property string label: index === 0 ? "1x" : index === 1 ? "2x" : "4x"
                width: parent.width
                height: strobe.rowHeight

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
                        x: row.phase * strobe.period - strobe.period
                        width: strobe.period * 40
                        height: parent.height
                        // One binding for the row rather than one per band: 120 opacity
                        // updates a frame is not a cost worth paying for a fade.
                        opacity: strobe.tuner.strobeAlpha

                        Repeater {
                            model: 40
                            delegate: Rectangle {
                                required property int index
                                x: index * strobe.period
                                width: strobe.period - 26
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
