import QtQuick
import Rigpa

// One 96×96 menu slot. An empty icon leaves the slot blank.
Item {
    id: button
    property url icon: ""
    property bool active: false
    signal clicked()

    width: Theme.menuWidth
    height: Theme.menuWidth

    Rectangle {
        anchors.fill: parent
        anchors.margins: 8
        radius: Theme.radius
        color: tap.pressed && button.enabled ? Theme.raised : button.active ? Theme.panel : "transparent"
        border.color: button.active ? Theme.line : "transparent"
    }
    Image {
        anchors.centerIn: parent
        source: button.icon
        visible: button.icon !== ""
        width: 64
        height: 64
        fillMode: Image.PreserveAspectFit
        sourceSize: Qt.size(128, 128)
        opacity: button.active ? 1 : 0.8
    }
    TapHandler {
        id: tap
        enabled: button.enabled
        onTapped: button.clicked()
    }
}
