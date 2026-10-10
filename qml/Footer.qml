import QtQuick
import Rigpa.Core

// What apply and back do here and now, and (from M2) the latest message.
Rectangle {
    color: Theme.background

    Rectangle { width: parent.width; height: 1; color: Theme.line }

    // Connection problems, while there are any.
    Text {
        anchors.right: parent.right
        anchors.rightMargin: 24
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width / 2
        horizontalAlignment: Text.AlignRight
        elide: Text.ElideRight
        visible: App.engine.connection !== EngineSession.Connected
        text: qsTr("Engine: ") + App.engine.connectionText
        color: App.engine.connection === EngineSession.Connecting ? Theme.dim : Theme.warn
        font.pixelSize: Theme.fontNormal
    }

    // The latest thing the rig had to say: a refusal, a missing model, a save that failed.
    Text {
        anchors.right: parent.right
        anchors.rightMargin: 24
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width * 0.6
        horizontalAlignment: Text.AlignRight
        elide: Text.ElideRight
        visible: App.engine.connection === EngineSession.Connected && App.rig.message !== ""
        text: App.rig.message
        color: Theme.warn
        font.pixelSize: Theme.fontNormal
    }

    Row {
        anchors.left: parent.left
        anchors.leftMargin: 24
        anchors.verticalCenter: parent.verticalCenter
        spacing: 32

        Repeater {
            model: [
                { key: qsTr("apply"), what: App.nav.applyHint },
                { key: qsTr("back"), what: App.nav.backHint }
            ]
            delegate: Row {
                required property var modelData
                spacing: 10
                opacity: modelData.what === "" ? 0.35 : 1
                Rectangle {
                    width: keyText.implicitWidth + 16
                    height: 26
                    radius: 6
                    color: Theme.raised
                    anchors.verticalCenter: parent.verticalCenter
                    Text {
                        id: keyText
                        anchors.centerIn: parent
                        text: modelData.key
                        color: Theme.dim
                        font.pixelSize: Theme.fontSmall
                    }
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData.what === "" ? "—" : modelData.what
                    color: Theme.text
                    font.pixelSize: Theme.fontNormal
                }
            }
        }
    }
}
