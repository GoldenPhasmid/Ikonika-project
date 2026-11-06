import QtQuick
import QtQuick.Layouts

DesktopCard
{
    id: root

    Layout.fillWidth: true
    visible: AppSyncController.needsReconnect
    padding: AppMetrics.gapMedium
    border.color: AppTheme.colourWrong

    Accessible.role: Accessible.AlertMessage
    Accessible.name: qsTr("Sync is paused")

    RowLayout
    {
        Layout.fillWidth: true
        spacing: AppMetrics.gapMedium

        DesktopIcon
        {
            name: "cloud_off"
            color: AppTheme.colourWrong
        }

        ColumnLayout
        {
            Layout.fillWidth: true
            spacing: AppMetrics.gapTight

            DesktopLabel
            {
                Layout.fillWidth: true
                text: qsTr("Sync is paused")
                font.weight: Font.DemiBold
                wrapMode: Text.WordWrap
            }

            DesktopLabel
            {
                Layout.fillWidth: true
                text: qsTr("Google asked you to sign in again. Your progress stays on this device until you reconnect.")
                font.pixelSize: AppMetrics.fontSmall
                color: AppTheme.colourSubtle
                wrapMode: Text.WordWrap
            }
        }

        DesktopButton
        {
            objectName: "today_reconnect"

            tone: "primary"
            text: qsTr("Reconnect")
            enabled: !AppSyncController.busy

            onClicked: AppSyncController.signIn()
        }
    }
}
