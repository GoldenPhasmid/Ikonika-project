import QtQuick
import QtQuick.Layouts

ColumnLayout
{
    id: root

    required property var exportFile
    required property var importFile
    required property var eraseSheet

    Layout.fillWidth: true
    spacing: AppMetrics.gapLarge

    MobileSettingsGroup
    {
        title: qsTr("DATA")

        MobileSettingsRow
        {
            title: qsTr("Export database")
            visible: Qt.platform.os !== "ios"

            onClicked:
            {
                if (Qt.platform.os === "android")
                {
                    AppSyncController.shareDatabase()
                    return
                }

                root.exportFile.selectedFile = "file:///ikonika-backup.db"
                root.exportFile.open()
            }

            MobileSettingsChevron {}
        }

        MobileSettingsRow
        {
            title: qsTr("Import database")
            lastRow: true

            onClicked: root.importFile.open()

            MobileSettingsChevron {}
        }
    }

    MobileSettingsGroup
    {
        title: qsTr("REMOVE")

        MobileSettingsRow
        {
            title: qsTr("Delete everything")
            description: qsTr("Every language, here and in your Google account")
            destructive: true
            lastRow: true

            onClicked: root.eraseSheet.openFresh()

            MobileSettingsChevron {}
        }
    }
}
