import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ColumnLayout
{
    id: root

    required property var lookup
    required property string term

    property int rowInset: -AppMetrics.gapSmall

    signal addRequested(string word, string translation)

    readonly property bool translated: root.lookup.machineTranslation.length > 0

    Layout.fillWidth: true
    spacing: AppMetrics.gapSmall

    DesktopLabel
    {
        Layout.fillWidth: true
        visible: root.lookup.formTerm.length === 0 && AppMachineTranslation.offered
        text: qsTr("Machine translation · %1").arg(AppMachineTranslation.pairLabel)
        font.weight: Font.DemiBold
        wrapMode: Text.WordWrap
    }

    DesktopWordRow
    {
        objectName: "machine_translation_form"

        Layout.fillWidth: true
        Layout.leftMargin: root.rowInset
        Layout.rightMargin: root.rowInset
        visible: root.lookup.formTerm.length > 0
        enabled: !root.lookup.translating && (root.translated || (AppMachineTranslation.offered && !AppMachineTranslation.downloading))
        word: root.lookup.formTerm
        translation: root.lookup.translating ? qsTr("Translating...")
            : root.translated ? root.lookup.machineTranslation
            : AppMachineTranslation.downloading ? qsTr("Downloading translation, %1%").arg(Math.round(AppMachineTranslation.progress * 100))
            : AppMachineTranslation.bytes > 0 ? qsTr("Download translation · %1 MB").arg(Math.round(AppMachineTranslation.bytes / (1024 * 1024)))
            : qsTr("Download translation")
        partOfSpeech: root.translated ? root.lookup.formReading : ""
        italicPartOfSpeech: false

        onClicked:
        {
            if (root.translated)
            {
                root.addRequested(root.lookup.formTerm, root.lookup.machineTranslation)

                return
            }

            AppMachineTranslation.download()
        }
    }

    Button
    {
        id: entry

        objectName: "machine_translation"

        Layout.fillWidth: true
        implicitHeight: Math.max(56, contentItem.implicitHeight + topPadding + bottomPadding)
        padding: AppMetrics.gapMedium
        visible: (root.lookup.translating || root.translated) && root.lookup.formTerm.length === 0
        enabled: root.translated && !root.lookup.translating
        hoverEnabled: true
        focusPolicy: Qt.StrongFocus
        text: root.term

        Accessible.name: qsTr("Add %1").arg(root.term)
        Accessible.description: root.lookup.machineTranslation

        onClicked: root.addRequested(root.term, root.lookup.machineTranslation)

        background: Rectangle
        {
            radius: AppMetrics.radiusMedium
            color: entry.down || entry.hovered ? AppTheme.colourFaint : AppTheme.colourPanel
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

    DesktopButton
    {
        objectName: "machine_translation_download"

        Layout.maximumWidth: root.width
        visible: root.lookup.formTerm.length === 0 && AppMachineTranslation.offered && !AppMachineTranslation.downloading
        tone: "primary"
        text: AppMachineTranslation.bytes > 0
            ? qsTr("Download translation · %1 MB").arg(Math.round(AppMachineTranslation.bytes / (1024 * 1024)))
            : qsTr("Download translation")

        onClicked: AppMachineTranslation.download()
    }

    DesktopProgressBar
    {
        Layout.fillWidth: true
        visible: root.lookup.formTerm.length === 0 && AppMachineTranslation.offered && AppMachineTranslation.downloading
        fill: AppTheme.colourAccent
        progress: AppMachineTranslation.progress
    }

    DesktopLabel
    {
        visible: root.lookup.formTerm.length === 0 && AppMachineTranslation.offered && AppMachineTranslation.downloading
        text: qsTr("Downloading translation, %1%").arg(Math.round(AppMachineTranslation.progress * 100))
        color: AppTheme.colourMuted
    }
}
