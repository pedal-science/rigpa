import QtQuick
import Rigpa

// A region the hardware controls can be in. `focused` draws the subtle edge that says so.
Rectangle {
    property bool focused: false
    color: Theme.panel
    radius: Theme.radius
    border.width: focused ? 2 : 1
    border.color: focused ? Qt.darker(Theme.accent, 1.4) : Theme.line
}
