import QtQuick
import Rigpa
import Rigpa.Core

// The tuner, as a main view: the note, how far off it is, and how loud the input is.
//
// While this is the view, rigpa is running a tuner node after each chain's input and that node's
// output is silence, so the rig is muted for exactly as long as this is on screen (App's
// mainViewChanged connection, and namchain docs/tuner.md). Back returns to the rig view, which
// removes the node again.
//
// It has no stops for the rotary: a tuner reports, it does not set. The ring is empty on purpose,
// so rotate and apply do nothing here and back is the one control (docs/spec.md's hardware table).
// Nothing animates while there is no note, so an idle tuner screen is a still screen.
Item {
    id: view
    readonly property var tuner: App.tuner
    readonly property FocusRing ring: App.nav.tuner
    readonly property bool active: App.nav.mainView === Navigator.TunerView
    readonly property bool hasNote: tuner.valid && tuner.noteText !== ""

    Component.onCompleted: {
        ring.kinds = [];
        ring.count = 0;
    }

    Panel {
        anchors.fill: parent
        anchors.margins: 12

        // The note: what the player is looking for, big enough to read from standing.
        Column {
            id: note
            x: 40
            anchors.verticalCenter: parent.verticalCenter
            width: 300
            spacing: 0
            Text {
                text: view.hasNote ? view.tuner.noteText : "\u2014"
                color: view.hasNote ? (view.tuner.inTune ? Theme.good : Theme.text) : Theme.faint
                font.pixelSize: 150
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
            x: note.x + note.width + 40
            anchors.verticalCenter: parent.verticalCenter
            width: view.tuner.barWidth
            height: 260

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
                    height: index === 5 ? 200 : index === 0 || index === 10 ? 60 : 30
                    color: index === 5 ? Theme.dim : Theme.line
                }
            }

            // In tune, drawn as a band rather than said in words.
            Rectangle {
                x: scale.mid - scale.inTuneWidth / 2
                anchors.verticalCenter: parent.verticalCenter
                width: scale.inTuneWidth
                height: 200
                radius: 4
                color: Theme.accentSoft
            }

            Rectangle {
                visible: view.hasNote
                x: view.tuner.needleX - width / 2
                anchors.verticalCenter: parent.verticalCenter
                width: 5
                height: 236
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

        // The numbers beside the picture: frequency, the level the engine hears, and the state line
        // that says the truth when there is no note -- or, worse, when the rig is not muted.
        Column {
            x: scale.x + scale.width + 48
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - x - 40
            spacing: 12

            Text {
                text: view.hasNote ? view.tuner.hzText : ""
                color: Theme.text
                font.pixelSize: Theme.fontLarge
                visible: text !== ""
            }

            // The input's level, from -60 dBFS up: whether anything is arriving at all.
            Item {
                width: Math.min(parent.width, 420)
                height: 18
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
                text: view.tuner.levelText
                color: Theme.dim
                font.pixelSize: Theme.fontSmall
            }

            Text {
                width: parent.width
                text: view.tuner.status
                color: Theme.warn
                font.pixelSize: Theme.fontNormal
                wrapMode: Text.WordWrap
                visible: text !== "" && !view.tuner.running
            }
            Text {
                text: qsTr("The rig is muted while this view is open")
                color: Theme.dim
                font.pixelSize: Theme.fontSmall
                visible: view.tuner.running
            }
            Text {
                text: view.tuner.a4Text
                color: Theme.faint
                font.pixelSize: Theme.fontSmall
                visible: text !== ""
            }
        }
    }
}
