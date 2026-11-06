import QtQuick
import QtQuick.Layouts

ColumnLayout
{
    id: root

    Layout.fillWidth: true
    spacing: AppMetrics.gapLarge

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
