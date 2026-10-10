import QtQuick
import QtQuick.Controls.Basic
import Rigpa
import Rigpa.Core

// Rig management's question: a name (typed on the virtual keyboard, which comes up below), or
// "are you sure" for a delete. OK and Cancel are the ring, so apply and back answer it too.
Rectangle {
    id: dialog
    readonly property var manager: App.manager
    readonly property FocusRing ring: manager.ring
    readonly property bool active: App.nav.overlay === Navigator.Dialog
    readonly property bool asksName: manager.dialog !== "delete"
    color: Theme.background

    Connections {
        target: dialog.manager
        function onDialogChanged() {
            if (dialog.manager.dialog === "") {
                field.focus = false
                return
            }
            field.text = dialog.manager.proposedName
            if (dialog.asksName) {
                field.forceActiveFocus()
                field.selectAll()
            }
        }
    }

    Column {
        x: 40; y: 18
        width: Math.min(1000, parent.width - 80)
        spacing: 14

        Text {
            width: parent.width
            elide: Text.ElideRight
            color: Theme.text
            font.pixelSize: Theme.fontLarge
            text: {
                const t = dialog.manager.dialogTarget
                switch (dialog.manager.dialog) {
                case "new": return qsTr("A new rig, called")
                case "rename": return qsTr("Rename %1 to").arg(t)
                case "duplicate": return qsTr("Copy %1 as").arg(t)
                case "saveAs": return qsTr("Keep this rig in the library as")
                case "delete": return qsTr("Delete %1 from the library?").arg(t)
                }
                return ""
            }
        }

        Row {
            spacing: 14
            TextField {
                id: field
                visible: dialog.asksName
                width: 640
                height: 52
                font.pixelSize: Theme.fontLarge
                color: Theme.text
                selectionColor: Theme.accent
                selectedTextColor: Theme.text
                inputMethodHints: Qt.ImhNoPredictiveText
                background: Rectangle {
                    radius: 8
                    color: Theme.raised
                    border.width: field.activeFocus ? 2 : 1
                    border.color: field.activeFocus ? Theme.accent : Theme.line
                }
                onTextChanged: dialog.manager.setProposedName(text)
                onAccepted: dialog.manager.confirm(text)
            }
            HeaderButton {
                anchors.verticalCenter: parent.verticalCenter
                label: dialog.manager.dialog === "delete" ? qsTr("Delete")
                     : dialog.manager.dialog === "rename" ? qsTr("Rename")
                     : dialog.manager.dialog === "new" ? qsTr("Create") : qsTr("Save")
                enabled: !dialog.manager.busy
                highlighted: dialog.active && dialog.ring.highlight === 0
                onClicked: dialog.manager.confirm(field.text)
            }
            HeaderButton {
                anchors.verticalCenter: parent.verticalCenter
                label: qsTr("Cancel")
                enabled: !dialog.manager.busy
                highlighted: dialog.active && dialog.ring.highlight === 1
                onClicked: dialog.manager.cancel()
            }
        }

        Text {
            width: parent.width
            wrapMode: Text.Wrap
            color: dialog.manager.dialogError !== "" ? Theme.warn : Theme.dim
            font.pixelSize: Theme.fontNormal
            text: dialog.manager.dialogError !== "" ? dialog.manager.dialogError
                : dialog.manager.dialog === "delete" ? qsTr("If it is playing it keeps playing until another rig is loaded. Undo can't bring it back.")
                : dialog.manager.busy ? qsTr("Asking the pedal…") : ""
        }
    }
}
