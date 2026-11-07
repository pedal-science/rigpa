import QtQuick
import Rigpa

// The tuner's note: what the player is looking for, big enough to read from standing, and how many
// cents off it is. Shared by both layouts' tuner views (TunerView, CompactTunerView).
Column {
    id: note
    required property var tuner
    required property bool hasNote
    property int noteSize: 132
    spacing: 0

    Text {
        text: note.hasNote ? note.tuner.noteText : "—"
        color: note.hasNote ? (note.tuner.inTune ? Theme.good : Theme.text) : Theme.faint
        font.pixelSize: note.noteSize
        font.weight: Font.DemiBold
    }
    Text {
        x: 6
        text: note.tuner.centsText
        color: note.tuner.inTune ? Theme.good : Theme.dim
        font.pixelSize: Theme.fontLarge
        visible: note.hasNote
    }
}
