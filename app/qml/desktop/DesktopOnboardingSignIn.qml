import QtQuick
import QtQuick.Layouts

DesktopPage
{
    id: root

    signal continued()

    maximumWidth: AppMetrics.dialogWidth
    centred: true

    DesktopLabel
    {
        Layout.fillWidth: true
        Layout.bottomMargin: AppMetrics.gapLarge
        text: qsTr("Ikonika")
        font.pixelSize: AppMetrics.brandTitleSize
        font.weight: Font.Medium
        horizontalAlignment: Text.AlignHCenter

        Accessible.role: Accessible.Heading
        Accessible.name: text
    }

    DesktopButton
    {
        objectName: "onboarding_sign_in"

        Layout.fillWidth: true
        large: true
        tone: "primary"
        text: AppSyncController.signedIn ? qsTr("Continue as %1").arg(AppSyncController.accountName)
                                         : qsTr("Sign in with Google")

        onClicked:
        {
            if (AppSyncController.signedIn)
            {
                root.continued()

                return
            }

            AppSyncController.signIn()
        }
    }

    DesktopButton
    {
        objectName: "onboarding_signed_out"

        Layout.fillWidth: true
        Layout.topMargin: -AppMetrics.gapSmall
        large: true
        tone: "ghost"
        text: qsTr("Continue signed out")

        onClicked: root.continued()
    }

    DesktopLabel
    {
        Layout.fillWidth: true
        visible: text.length > 0
        text: AppSyncController.statusText
        font.pixelSize: AppMetrics.fontSmall
        color: AppTheme.colourMuted
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
    }
}
