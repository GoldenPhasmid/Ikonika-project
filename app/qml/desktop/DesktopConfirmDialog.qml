import QtQuick

DesktopDialog
{
    id: root

    property string confirmLabel: qsTr("Delete")
    property string cancelLabel: qsTr("Cancel")
    property bool destructive: true
    property bool safeDefault: false
    property var action: null

    dismissable: false
    titleAlignment: Text.AlignHCenter

    function ask(title, message, confirmLabel, action)
    {
        root.title = title
        root.message = message
        root.confirmLabel = confirmLabel
        root.action = action
        root.open()
    }

    function confirm()
    {
        const action = root.action

        root.close()

        if (action)
        {
            action()
        }
    }

    onOpened: (root.safeDefault ? trailingButton : leadingButton).forceActiveFocus(Qt.TabFocusReason)

    footer: [
        DesktopButton
        {
            id: leadingButton

            objectName: "confirm_dialog_leading"

            large: true
            text: root.safeDefault ? root.confirmLabel : root.cancelLabel

            onClicked: root.safeDefault ? root.confirm() : root.close()
        },
        DesktopButton
        {
            id: trailingButton

            objectName: "confirm_dialog_trailing"

            large: true
            text: root.safeDefault ? root.cancelLabel : root.confirmLabel
            tone: root.safeDefault ? "primary" : (root.destructive ? "danger" : "primary")

            onClicked: root.safeDefault ? root.close() : root.confirm()
        }
    ]
}
