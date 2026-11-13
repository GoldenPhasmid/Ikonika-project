import QtQuick
import QtQuick.Layouts

Rectangle
{
    id: root

    signal continued()

    color: AppTheme.colourPanel

    ColumnLayout
    {
        anchors.fill: parent
        spacing: 0

        MobileHeader
        {
            showBack: AppOnboarding.canGoBack

            onBackRequested: AppOnboarding.back()
        }

        ColumnLayout
        {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Item { Layout.fillHeight: true }

            Text
            {
                Layout.alignment: Qt.AlignHCenter
                Layout.bottomMargin: 62

                text: qsTr("Ikonika")
                font.pixelSize: 56
                font.weight: Font.Medium
                font.family: AppMetrics.fontFamily
                color: AppTheme.colourText
            }

            ColumnLayout
            {
                Layout.fillWidth: true
                Layout.leftMargin: 66
                Layout.rightMargin: 66
                spacing: AppMetrics.gapCompact

                MobileWideButton
                {
                    label: AppSyncController.signedIn
                        ? qsTr("Continue as %1").arg(AppSyncController.accountName)
                        : qsTr("Sign in with Google")

                    interactive: !AppSyncController.busy

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

                MobileWideButton
                {
                    label: qsTr("Continue signed out")
                    filled: false

                    onClicked: root.continued()
                }
            }

            Text
            {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: AppMetrics.gapMedium

                text: AppSyncController.statusText
                visible: text.length > 0
                font.pixelSize: AppMetrics.fontSmall
                font.family: AppMetrics.fontFamily
                color: AppTheme.colourMuted
            }

            Item { Layout.fillHeight: true }
        }
    }
}
