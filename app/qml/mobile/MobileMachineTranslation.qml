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

    MobileWordCard
    {
        objectName: "machine_translation_form"

        Layout.fillWidth: true
        visible: root.lookup.formTerm.length > 0
        enabled: !root.lookup.translating && (root.translated || (AppMachineTranslation.offered && !AppMachineTranslation.downloading))
        word: root.lookup.formTerm
        translation: root.lookup.translating ? qsTr("Translating...")
            : root.translated ? root.lookup.machineTranslation
            : AppMachineTranslation.downloading ? qsTr("Downloading translation, %1%").arg(Math.round(AppMachineTranslation.progress * 100))
            : AppMachineTranslation.bytes > 0 ? qsTr("Download translation · %1 MB").arg(Math.round(AppMachineTranslation.bytes / (1024 * 1024)))
            : qsTr("Download translation")
        partOfSpeech: root.translated ? root.lookup.formReading : ""
        maxStage: 1

        showVoice: false
        showProgress: false
        showCefr: false
        showEditAction: false
        showDeleteAction: false
        showPartOfSpeech: true

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
        padding: AppMetrics.contentPadding
        visible: (root.lookup.translating || root.translated) && root.lookup.formTerm.length === 0
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
        visible: root.lookup.formTerm.length === 0 && AppMachineTranslation.offered && !AppMachineTranslation.downloading

        label: AppMachineTranslation.bytes > 0
            ? qsTr("Download · %1 MB").arg(Math.round(AppMachineTranslation.bytes / (1024 * 1024)))
            : qsTr("Download")

        onClicked: AppMachineTranslation.download()
    }

    ColumnLayout
    {
        Layout.fillWidth: true
        visible: root.lookup.formTerm.length === 0 && AppMachineTranslation.offered && AppMachineTranslation.downloading
        spacing: AppMetrics.gapSmall

        Text
        {
            Layout.fillWidth: true
            text: qsTr("Downloading...")
            color: AppTheme.colourMuted
            font.pixelSize: AppMetrics.fontBody
            font.family: AppMetrics.fontFamily
            horizontalAlignment: Text.AlignHCenter
        }

        ProgressBar
        {
            Layout.fillWidth: true
            Accessible.name: qsTr("Downloading translation")
            value: AppMachineTranslation.progress
            indeterminate: value <= 0
            palette.dark: AppTheme.colourText
            palette.midlight: AppTheme.colourFaint
        }
    }
}
