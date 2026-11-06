import QtQuick
import QtQuick.Layouts
import "MobileIcons.js" as Icons

MobileCard
{
    id: root

    visible: AppSyncController.needsReconnect
    border.color: AppTheme.colourWrong
    spacing: AppMetrics.gapMedium

    Accessible.role: Accessible.AlertMessage
    Accessible.name: qsTr("Sync is paused")

    RowLayout
    {
        Layout.fillWidth: true
        spacing: AppMetrics.gapSmall

        MobileVectorIcon
        {
            Layout.alignment: Qt.AlignTop
            width: AppMetrics.headerActionIconSize
            height: AppMetrics.headerActionIconSize
            svgPath: Icons.Icons.cloud_off.path
            color: AppTheme.colourWrong
        }

        ColumnLayout
        {
            Layout.fillWidth: true
            spacing: AppMetrics.gapTight

            Text
            {
                Layout.fillWidth: true

                text: qsTr("Sync is paused")
                font.pixelSize: AppMetrics.fontBody
                font.family: AppMetrics.fontFamily
                font.weight: Font.DemiBold
                color: AppTheme.colourText
                wrapMode: Text.WordWrap
            }

            Text
            {
                Layout.fillWidth: true

                text: qsTr("Google asked you to sign in again. Your progress stays on this device until you reconnect.")
                font.pixelSize: AppMetrics.fontMeta
                font.family: AppMetrics.fontFamily
                color: AppTheme.colourSubtle
                wrapMode: Text.WordWrap
            }
        }
    }

    MobileWideButton
    {
        objectName: "today_reconnect"

        label: qsTr("Reconnect")
        filled: true
        interactive: !AppSyncController.busy

        onClicked: AppSyncController.signIn()
    }
}
