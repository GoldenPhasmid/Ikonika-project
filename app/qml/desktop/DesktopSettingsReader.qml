import QtQuick
import QtQuick.Layouts

ColumnLayout
{
    id: root

    Layout.fillWidth: true
    spacing: AppMetrics.gapLarge

    DesktopSettingsGroup
    {
        title: qsTr("Library")

        DesktopSettingsRow
        {
            leftPadding: AppMetrics.gapLarge
            rightPadding: AppMetrics.gapLarge
            text: qsTr("Upload imported books to the cloud")
            description: qsTr("Books up to 15 MB, while you are signed in")
            lastRow: true

            DesktopToggle
            {
                objectName: "reader_upload_imported"

                accessibleName: qsTr("Upload imported books to the cloud")
                on: AppReaderSettings.uploadImportedBooks

                onClicked: AppReaderSettings.uploadImportedBooks = !AppReaderSettings.uploadImportedBooks
            }
        }
    }

    DesktopSettingsGroup
    {
        title: qsTr("PDF")

        DesktopReaderSettingsRows
        {
            showText: false
        }
    }

    DesktopSettingsGroup
    {
        title: qsTr("EPUB, FB2, TXT")

        DesktopReaderSettingsRows
        {
            showPdf: false
        }
    }
}
