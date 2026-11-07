import QtQuick
import QtQuick.VirtualKeyboard
import QtQuick.Window
import Rigpa.Core

// The window, and the one transform between the layout's design (1920×480, or 800×480 for the
// compact layout) and the real screen: the bar is natively 480×1920, so its screen is rotated
// (display.rotation); on the Mac the design is scaled to fit the window, which opens at
// display.scale (0 = fit its screen) and can be resized or made full screen — a fixed size is what
// greys out macOS's green button. Touch follows the transform by itself.
Window {
    id: window

    readonly property bool onPedal: Qt.platform.pluginName === "eglfs"
    readonly property int rotation: App.config.rotation
    readonly property bool sideways: rotation === 90 || rotation === 270
    readonly property real designWidth: sideways ? Theme.screenHeight : Theme.screenWidth
    readonly property real designHeight: sideways ? Theme.screenWidth : Theme.screenHeight
    // The design scaled to fit the window, aspect kept, centred: the rest is letterbox.
    readonly property real screenScale: onPedal ? 1 : Math.min(width / designWidth, height / designHeight)

    minimumWidth: designWidth / 4
    minimumHeight: designHeight / 4
    title: "rigpa"
    color: Theme.menuBackground

    // Sized once, not bound: a binding to Screen would resize the window, full screen or not,
    // whenever it moved to another display. Screen.width, not desktopAvailableWidth, which spans
    // every display, so a second one made the window wider than its own screen.
    Component.onCompleted: {
        if (onPedal) {
            visibility = Window.FullScreen
            return
        }
        const scale = App.config.scale > 0 ? App.config.scale
            : Math.min(1, (Screen.width - 48) / designWidth,
                          (Screen.height - 96) / designHeight)
        width = designWidth * scale
        height = designHeight * scale
        visibility = Window.Windowed
    }

    Item {
        width: Theme.screenWidth
        height: Theme.screenHeight
        // The layout's set of views: the bar's, or the 4.3" panel's (LayoutSpec, display.layout).
        Loader {
            anchors.fill: parent
            sourceComponent: App.layout.compact ? compactShell : wideShell
        }
        Component { id: wideShell; Shell {} }
        Component { id: compactShell; CompactShell {} }
        // Qt Virtual Keyboard, along the bottom while a text field has focus.
        InputPanel {
            id: keyboard
            z: 100
            // 880 on the bar (about 276 tall) and the panel's whole 800 (about 250): either way clear
            // of a dialog's name row above it.
            width: App.layout.keyboardWidth
            x: (parent.width - width) / 2
            y: active ? parent.height - height : parent.height
            visible: active
        }
        transform: [
            Rotation { angle: window.rotation },
            Translate {
                x: window.rotation === 90 ? Theme.screenHeight : window.rotation === 180 ? Theme.screenWidth : 0
                y: window.rotation === 180 ? Theme.screenHeight : window.rotation === 270 ? Theme.screenWidth : 0
            },
            Scale { xScale: window.screenScale; yScale: window.screenScale },
            Translate {
                x: (window.width - window.designWidth * window.screenScale) / 2
                y: (window.height - window.designHeight * window.screenScale) / 2
            }
        ]
    }
}
