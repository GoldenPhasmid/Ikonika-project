import QtQuick
import QtQuick.Layouts

MobileBottomSheet
{
    id: root

    property string message: ""
    property string confirmLabel: ""
    property var action: null

    visible: false

    onDismissed: root.close()

    function ask(heading, message, confirmLabel, action)
    {
        root.heading = heading
        root.message = message
        root.confirmLabel = confirmLabel
        root.action = action
        root.present()
    }

    function close()
    {
        root.hide()
    }

    Text
    {
        Layout.fillWidth: true

        text: root.message
        color: AppTheme.colourSubtle
        font.pixelSize: AppMetrics.fontBody
        font.family: AppMetrics.fontFamily
        wrapMode: Text.WordWrap
    }

    MobileButtonPair
    {
        objectName: "confirm_sheet_buttons"

        leftLabel: qsTr("Cancel")
        rightLabel: root.confirmLabel
        rightDestructive: true

        onLeftClicked: root.close()
        onRightClicked:
        {
            const action = root.action

            root.close()

            if (action !== null)
            {
                action()
            }
        }
    }
}
