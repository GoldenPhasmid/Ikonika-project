import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ColumnLayout
{
    id: root

    required property var lookup
    required property string term

    signal addRequested(string word, string translation)

    readonly property bool translated: root.lookup.machineTranslation.length > 0

    spacing: AppMetrics.gapSmall

    Button
    {
        id: entry

        objectName: "machine_translation"

        Layout.fillWidth: true
        implicitHeight: Math.max(56, contentItem.implicitHeight + topPadding + bottomPadding)
        padding: AppMetrics.contentPadding
        visible: root.lookup.translating || root.translated
        enabled: root.translated && !root.lookup.translating
        text: root.term

        Accessible.name: qsTr("Add %1").arg(root.term)
        Accessible.description: root.lookup.machineTranslation

        onClicked: root.addRequested(root.term, root.lookup.machineTranslation)

        background: Rectangle
        {
            radius: AppMetrics.radiusMedium
            color: entry.down ? AppTheme.colourFaint : AppTheme.colourPanel
            border.width: AppMetrics.borderWidth
            border.color: entry.visualFocus ? AppTheme.colourAccent : AppTheme.colourBorder
        }

        contentItem: ColumnLayout
        {
            spacing: AppMetrics.gapSmall

            Text
            {
                Layout.fillWidth: true
                text: root.lookup.translating ? qsTr("Translating...") : root.lookup.machineTranslation
                color: root.lookup.translating ? AppTheme.colourMuted : AppTheme.colourText
                font.pixelSize: AppMetrics.fontLead
                font.family: AppMetrics.fontFamily
                wrapMode: Text.WordWrap
                textFormat: Text.PlainText
            }
        }
    }

    MobileWideButton
    {
        objectName: "machine_translation_download"

        Layout.fillWidth: true
        visible: AppMachineTranslation.offered && !AppMachineTranslation.downloading

        label: AppMachineTranslation.bytes > 0
            ? qsTr("Download · %1 MB").arg(Math.round(AppMachineTranslation.bytes / (1024 * 1024)))
            : qsTr("Download")

        onClicked: AppMachineTranslation.download()
    }

    Text
    {
        Layout.fillWidth: true
        visible: AppMachineTranslation.offered && AppMachineTranslation.downloading

        text: qsTr("Downloading...")
        color: AppTheme.colourMuted
        font.pixelSize: AppMetrics.fontBody
        font.family: AppMetrics.fontFamily
        horizontalAlignment: Text.AlignHCenter
    }
}
