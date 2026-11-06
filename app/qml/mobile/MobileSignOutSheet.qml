import QtQuick
import QtQuick.Layouts

MobileBottomSheet
{
    id: root

    heading: qsTr("Sign out?")

    visible: false

    onDismissed: root.close()

    function open()
    {
        root.present()
    }

    function close()
    {
        root.hide()
    }

    Text
    {
        Layout.fillWidth: true

        text: qsTr("Your words and progress stay in your Google account. Sign in again to pick up where you left off.")
        color: AppTheme.colourSubtle
        font.pixelSize: AppMetrics.fontBody
        font.family: AppMetrics.fontFamily
        wrapMode: Text.WordWrap
    }

    MobileButtonPair
    {
        leftLabel: qsTr("Cancel")
        rightLabel: qsTr("Sign out")
        rightDestructive: true

        onLeftClicked: root.close()
        onRightClicked:
        {
            root.close()
            AppSyncController.signOut()
        }
    }
}
