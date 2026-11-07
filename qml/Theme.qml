pragma Singleton
import QtQuick
import Rigpa.Core

// The look, in one place. Sizes are the layout's (LayoutSpec: the spec's 1920×480 bar, or the
// 800×480 panel), in pixels of that screen; on the Mac the whole screen is scaled, never these.
QtObject {
    readonly property int screenWidth: App.layout.screenWidth
    readonly property int screenHeight: App.layout.screenHeight
    readonly property int menuWidth: App.layout.menuWidth
    readonly property int headerHeight: App.layout.headerHeight
    readonly property int footerHeight: App.layout.footerHeight
    readonly property int infoWidth: App.layout.infoWidth
    readonly property int contentHeight: screenHeight - headerHeight - footerHeight // 368 wide, 388 compact
    readonly property int mainWidth: screenWidth - menuWidth - infoWidth             // 1504 wide, 800 compact

    readonly property color background: "#121212"
    readonly property color menuBackground: "#0b0b0b"
    readonly property color panel: "#1a1a1a"
    readonly property color raised: "#242424"
    readonly property color line: "#2e2e2e"
    readonly property color text: "#e6e6e6"
    readonly property color dim: "#8c8c8c"
    readonly property color faint: "#5a5a5a"
    // The orange pedal of the icon: what is highlighted, and what is being edited.
    readonly property color accent: "#e08050"
    readonly property color accentSoft: Qt.rgba(0.878, 0.502, 0.314, 0.18)
    readonly property color good: "#7cc47c"
    readonly property color warn: "#e0c050"
    readonly property color bad: "#e05050"

    readonly property int radius: 10
    readonly property int highlightWidth: 3
    readonly property int fontSmall: 15
    readonly property int fontNormal: 19
    readonly property int fontLarge: 24
    readonly property int fontTitle: 28

    function icon(name) { return Qt.resolvedUrl("../resources/icons/" + name + ".svg") }
}
