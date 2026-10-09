import QtQuick
import QtQuick.Window
import Rigpa.Core

// The window, and the one transform between the 1920×480 design and the real screen: on the
// pedal the panel is natively 480×1920, so the screen is rotated (display.rotation); on the Mac it
// is scaled to fit (display.scale, 0 = fit). Touch follows the transform by itself.
Window {
    id: window

    readonly property bool onPedal: Qt.platform.pluginName === "eglfs"
    readonly property int rotation: App.config.rotation
    readonly property bool sideways: rotation === 90 || rotation === 270
    readonly property real fitScale: Math.min(1, (Screen.desktopAvailableWidth - 48) / Theme.screenWidth)
    readonly property real screenScale: onPedal ? 1 : (App.config.scale > 0 ? App.config.scale : fitScale)

    width: (sideways ? Theme.screenHeight : Theme.screenWidth) * screenScale
    height: (sideways ? Theme.screenWidth : Theme.screenHeight) * screenScale
    minimumWidth: width; maximumWidth: width
    minimumHeight: height; maximumHeight: height
    visibility: onPedal ? Window.FullScreen : Window.Windowed
    visible: true
    title: "rigpa"
    color: Theme.menuBackground

    Shell {
        width: Theme.screenWidth
        height: Theme.screenHeight
        transform: [
            Rotation { angle: window.rotation },
            Translate {
                x: window.rotation === 90 ? Theme.screenHeight : window.rotation === 180 ? Theme.screenWidth : 0
                y: window.rotation === 180 ? Theme.screenHeight : window.rotation === 270 ? Theme.screenWidth : 0
            },
            Scale { xScale: window.screenScale; yScale: window.screenScale }
        ]
    }
}
