import QtQuick
import QtQuick.Shapes
import Rigpa

// A rotary knob for one parameter: a 270° arc filled to `position` (0–1). Drag up or down to
// turn it (the whole travel is `travel` pixels); with the hardware, highlight it, press apply to
// select it, and turn the rotary. `editing` (selected) fills it with the accent colour.
Item {
    id: knob
    property real position: 0
    property string label
    property string valueText
    property bool highlighted: false
    property bool editing: false
    property real diameter: 76
    property real travel: 220
    signal moved(real position)    // touch: where the drag has taken it
    signal released()
    signal pressed()

    implicitWidth: Math.max(diameter + 24, 116)
    implicitHeight: diameter + 54

    readonly property real startAngle: 135
    readonly property real sweep: 270

    Item {
        id: dial
        width: knob.diameter
        height: knob.diameter
        anchors.horizontalCenter: parent.horizontalCenter

        Rectangle { // the highlight
            anchors.centerIn: parent
            width: parent.width + 12
            height: width
            radius: width / 2
            color: knob.editing ? Theme.accentSoft : "transparent"
            border.width: knob.highlighted ? Theme.highlightWidth : 0
            border.color: Theme.accent
        }

        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer
            ShapePath { // the track
                strokeColor: Theme.raised
                strokeWidth: 8
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                PathAngleArc {
                    centerX: dial.width / 2; centerY: dial.height / 2
                    radiusX: dial.width / 2 - 6; radiusY: dial.height / 2 - 6
                    startAngle: knob.startAngle
                    sweepAngle: knob.sweep
                }
            }
            ShapePath { // the value
                strokeColor: knob.editing ? Theme.accent : Qt.darker(Theme.accent, 1.25)
                strokeWidth: 8
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                PathAngleArc {
                    centerX: dial.width / 2; centerY: dial.height / 2
                    radiusX: dial.width / 2 - 6; radiusY: dial.height / 2 - 6
                    startAngle: knob.startAngle
                    sweepAngle: Math.max(0.5, knob.sweep * knob.position)
                }
            }
        }

        Rectangle { // the pointer
            width: 4
            height: dial.height / 2 - 14
            radius: 2
            color: Theme.text
            x: dial.width / 2 - width / 2
            y: 14 // its bottom is the centre, which it turns about
            transformOrigin: Item.Bottom
            rotation: knob.startAngle + knob.sweep * knob.position + 90
        }

        DragHandler {
            id: drag
            target: null
            xAxis.enabled: false
            property real startPosition: 0
            onActiveChanged: {
                if (active) {
                    startPosition = knob.position
                    knob.pressed()
                } else {
                    knob.released()
                }
            }
            onTranslationChanged: if (active) knob.moved(Math.max(0, Math.min(1, startPosition - translation.y / knob.travel)))
        }
        TapHandler { onTapped: knob.pressed() }
    }

    Text {
        id: valueLabel
        y: knob.diameter + 6
        width: parent.width
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
        text: knob.valueText
        color: knob.editing ? Theme.accent : Theme.text
        font.pixelSize: 16
    }
    Text {
        anchors.top: valueLabel.bottom
        anchors.topMargin: 2
        width: parent.width
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
        text: knob.label
        color: Theme.dim
        font.pixelSize: 14
    }
}
