pragma Singleton
import QtQuick

// The look, in one place. Sizes are the spec's (docs/spec.md, Layout), in pixels of the 1920×480
// screen; on the Mac the whole screen is scaled, never these.
QtObject {
    readonly property int screenWidth: 1920
    readonly property int screenHeight: 480
    readonly property int menuWidth: 96
    readonly property int headerHeight: 64
    readonly property int footerHeight: 48
    readonly property int infoWidth: 320
    readonly property int contentHeight: screenHeight - headerHeight - footerHeight // 368
    readonly property int mainWidth: screenWidth - menuWidth - infoWidth             // 1504

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
