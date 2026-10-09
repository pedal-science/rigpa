import QtQuick
import Rigpa

// A choice between a few labels. Apply on it (it is a "toggle" in its ring) moves to the next.
Rectangle {
    id: segmented
    property var labels: []
    property int current: 0
    property bool highlighted: false
    property int segmentWidth: 120
    property int fontSize: Theme.fontNormal
    signal chosen(int index)

    implicitWidth: row.implicitWidth + 8
    height: 44
    radius: 10
    color: Theme.raised
    border.width: highlighted ? Theme.highlightWidth : 0
    border.color: Theme.accent

    Row {
        id: row
        anchors.centerIn: parent
        Repeater {
            model: segmented.labels
            delegate: Rectangle {
                required property int index
                required property string modelData
                width: Math.max(segmented.segmentWidth, label.implicitWidth + 24)
                height: 36
                radius: 8
                color: index === segmented.current ? Theme.panel : "transparent"
                Text {
                    id: label
                    anchors.centerIn: parent
                    text: parent.modelData
                    color: parent.index === segmented.current ? Theme.text : Theme.dim
                    font.pixelSize: segmented.fontSize
                }
                TapHandler { onTapped: segmented.chosen(parent.index) }
            }
        }
    }
}
