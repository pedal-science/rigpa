import QtQuick
import Rigpa

// The tuner's cents scale: half a semitone either side of the note, the in-tune band, and the
// needle. Its width is the model's bar width (TunerModel, which places the needle in those pixels);
// its height is the layout's, and everything drawn in it scales with that height. Shared by both
// layouts' tuner views.
//
// The id is `meter`, not `scale`: inside the ticks' delegate `scale` is each tick's own Item.scale
// (1.0), which shadows an id from outside the delegate, so every tick was placed at NaN, drawn at
// x 0. That is how the bar's ticks were drawn until 2026-11-07: stacked at the scale's left end.
Item {
    id: meter
    required property var tuner
    required property bool hasNote
    width: tuner.barWidth
    height: 200

    readonly property real mid: width / 2
    readonly property real halfBar: width / 2
    readonly property real inTuneWidth: width * (tuner.inTuneCents / tuner.maxCents)
    // At the bar's 200 px these are its 160, 190, 48 and 24.
    readonly property int bandHeight: Math.round(height * 4 / 5)
    readonly property int needleHeight: Math.round(height * 19 / 20)
    readonly property int endTick: Math.round(height * 6 / 25)
    readonly property int tick: Math.round(height * 3 / 25)

    // A tick every tenth of the way, taller at the centre and at the ends.
    Repeater {
        model: 11
        delegate: Rectangle {
            required property int index
            readonly property real cents: (index - 5) * (meter.tuner.maxCents / 5)
            x: meter.mid + cents / meter.tuner.maxCents * meter.halfBar - width / 2
            anchors.verticalCenter: parent.verticalCenter
            width: index === 5 ? 3 : 1
            height: index === 5 ? meter.bandHeight : index === 0 || index === 10 ? meter.endTick : meter.tick
            color: index === 5 ? Theme.dim : Theme.line
        }
    }

    // In tune, drawn as a band rather than said in words.
    Rectangle {
        x: meter.mid - meter.inTuneWidth / 2
        anchors.verticalCenter: parent.verticalCenter
        width: meter.inTuneWidth
        height: meter.bandHeight
        radius: 4
        color: Theme.accentSoft
    }

    Rectangle {
        visible: meter.hasNote
        x: meter.tuner.needleX - width / 2
        anchors.verticalCenter: parent.verticalCenter
        width: 5
        height: meter.needleHeight
        radius: 2
        color: meter.tuner.inTune ? Theme.good : Theme.accent
        Behavior on x { NumberAnimation { duration: 40 } }
    }

    Text {
        anchors.left: parent.left
        anchors.top: parent.top
        text: "−" + meter.tuner.maxCents
        color: Theme.faint
        font.pixelSize: Theme.fontSmall
    }
    Text {
        anchors.right: parent.right
        anchors.top: parent.top
        text: "+" + meter.tuner.maxCents
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
