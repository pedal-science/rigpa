import QtQuick
import Rigpa

Rectangle {
    id: toggle
    property bool checked: false
    property bool highlighted: false
    signal toggled()

    width: 88
    height: 44
    radius: height / 2
    color: checked ? Theme.accent : Theme.raised
    border.width: highlighted ? Theme.highlightWidth : 0
    border.color: checked ? Theme.text : Theme.accent

    Rectangle {
        width: 34; height: 34; radius: 17
        y: 5
        x: toggle.checked ? toggle.width - width - 5 : 5
        color: Theme.text
        Behavior on x { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
    }
    TapHandler { onTapped: toggle.toggled() }
}
