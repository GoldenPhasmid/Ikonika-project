import QtQuick
import QtQuick.Layouts

Rectangle
{
    id: root

    color: AppTheme.colourPanel

    ColumnLayout
    {
        anchors.fill: parent
        spacing: 0

        MobileHeader {}

        ColumnLayout
        {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: AppMetrics.gapLarge
            spacing: AppMetrics.gapLarge

            Text
            {
                Layout.fillWidth: true

                text: qsTr("Restoring your words")
                font.pixelSize: AppMetrics.fontHeadline
                font.weight: Font.Medium
                font.family: AppMetrics.fontFamily
                color: AppTheme.colourText
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Text
            {
                Layout.fillWidth: true

                text: qsTr("Your cards and progress are coming from %1.").arg(AppSyncController.accountEmail)
                font.pixelSize: AppMetrics.fontBody
                font.family: AppMetrics.fontFamily
                color: AppTheme.colourSubtle
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Item
            {
                Layout.fillHeight: true
            }
        }
    }
}
