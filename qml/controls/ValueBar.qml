import QtQuick
import Rigpa

// A non-boolean control as a bar: drag or tap along it to set the value, or highlight it with
// the rotary, press apply to select it and turn the rotary to step it.
Item {
    id: bar
    property real from: 0
    property real to: 100
    property real value: 0
    property string text: Math.round(value)
    property bool highlighted: false
    property bool editing: false
    signal moved(real value)       // touch: the value under the finger
    signal pressed()

    implicitHeight: 56

    Rectangle {
        id: track
        anchors.left: parent.left
        anchors.right: valueText.left
        anchors.rightMargin: 20
        anchors.verticalCenter: parent.verticalCenter
        height: 40
        radius: 8
        color: Theme.raised
        border.width: bar.highlighted ? Theme.highlightWidth : 0
        border.color: Theme.accent

        Rectangle {
            x: 4; y: 4
            height: parent.height - 8
            width: Math.max(0, (parent.width - 8) * (bar.value - bar.from) / (bar.to - bar.from))
            radius: 5
            color: bar.editing ? Theme.accent : Qt.darker(Theme.accent, 1.6)
        }

        DragHandler {
            target: null
            xAxis.enabled: true
            yAxis.enabled: false
            onActiveChanged: if (active) bar.pressed()
            onCentroidChanged: if (active) bar.moved(bar.valueAt(centroid.position.x))
        }
        TapHandler {
            onTapped: (point) => { bar.pressed(); bar.moved(bar.valueAt(point.position.x)) }
        }
    }

    Text {
        id: valueText
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        width: 110
        horizontalAlignment: Text.AlignRight
        text: bar.text
        color: bar.editing ? Theme.accent : Theme.text
        font.pixelSize: Theme.fontLarge
    }

    function valueAt(x) {
        const f = Math.max(0, Math.min(1, (x - 4) / (track.width - 8)))
        return from + f * (to - from)
    }
}
