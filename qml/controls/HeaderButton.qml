import QtQuick
import Rigpa

Rectangle {
    id: button
    property string label
    property bool highlighted: false
    property color base: Theme.panel
    signal clicked()

    width: Math.max(48, labelText.implicitWidth + 28)
    height: 44
    radius: 8
    color: tap.pressed ? Qt.lighter(base, 1.3) : base
    opacity: enabled ? 1 : 0.35
    border.width: highlighted ? Theme.highlightWidth : 0
    border.color: Theme.accent
    Text { id: labelText; anchors.centerIn: parent; text: button.label; color: Theme.text; font.pixelSize: Theme.fontNormal }
    TapHandler { id: tap; onTapped: button.clicked() }
}
