import QtQuick
import QtQuick.Layouts

DesktopFormPage
{
    id: root

    maximumWidth: AppMetrics.dialogWidth

    DesktopLabel
    {
        Layout.fillWidth: true

        text: qsTr("Restoring your words")
        font.pixelSize: AppMetrics.fontHeadline
        font.weight: Font.DemiBold
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap

        Accessible.role: Accessible.Heading
        Accessible.name: text
    }

    DesktopLabel
    {
        Layout.fillWidth: true

        text: qsTr("Your cards and progress are coming from %1.").arg(AppSyncController.accountEmail)
        color: AppTheme.colourSubtle
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
    }
}
