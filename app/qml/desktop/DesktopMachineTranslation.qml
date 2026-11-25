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
        visible: root.lookup.formTerm.length === 0 && root.lookup.machinePack.offered
        text: qsTr("Machine translation · %1").arg(root.lookup.machinePack.pairLabel)
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
        enabled: !root.lookup.translating && (root.translated || (root.lookup.machinePack.offered && !root.lookup.machinePack.downloading))
        word: root.lookup.formTerm
        translation: root.lookup.translating ? qsTr("Translating...")
            : root.translated ? root.lookup.machineTranslation
            : root.lookup.machinePack.downloading ? qsTr("Downloading translation, %1%").arg(Math.round(root.lookup.machinePack.progress * 100))
            : root.lookup.machinePack.bytes > 0 ? qsTr("Download translation · %1 MB").arg(Math.round(root.lookup.machinePack.bytes / (1024 * 1024)))
            : qsTr("Download translation")
        partOfSpeech: root.translated ? root.lookup.formReading : ""

        onClicked:
        {
            if (root.translated)
            {
                root.addRequested(root.lookup.formTerm, root.lookup.machineTranslation)

                return
            }

            root.lookup.machinePack.download()
        }
    }

    Rectangle
    {
        id: entry

        objectName: "machine_translation"

        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 56
        Layout.preferredHeight: translationText.implicitHeight + AppMetrics.gapMedium * 2
        visible: (root.lookup.translating || root.translated) && root.lookup.formTerm.length === 0
        radius: AppMetrics.radiusMedium
        color: AppTheme.colourPanel
        border.width: AppMetrics.borderWidth
        border.color: AppTheme.colourBorder
        clip: true

        Accessible.name: qsTr("Translation")
        Accessible.description: root.lookup.machineTranslation

        Flickable
        {
            id: scroller

            anchors.fill: parent
            anchors.margins: AppMetrics.gapMedium
            contentWidth: width
            contentHeight: translationText.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            clip: true

            ScrollBar.vertical: DesktopScrollBar {}

            TextEdit
            {
                id: translationText

                width: scroller.width - AppMetrics.gapMedium
                readOnly: true
                selectByMouse: true
                persistentSelection: true
                text: root.lookup.machineTranslation.length > 0 ? root.lookup.machineTranslation : qsTr("Translating...")
                color: root.lookup.translating ? AppTheme.colourMuted : AppTheme.colourText
                selectionColor: AppTheme.colourAccent
                font.pixelSize: AppMetrics.fontLead
                font.family: AppMetrics.fontFamily
                wrapMode: TextEdit.Wrap
                textFormat: TextEdit.PlainText
            }
        }
    }

    DesktopButton
    {
        objectName: "machine_translation_add"

        visible: entry.visible && root.translated && !root.lookup.translating
        iconName: "plus"
        text: qsTr("Add")

        Accessible.description: qsTr("Add %1").arg(root.term)

        onClicked: root.addRequested(root.term, root.lookup.machineTranslation)
    }

    DesktopButton
    {
        objectName: "machine_translation_download"

        Layout.maximumWidth: root.width
        visible: root.lookup.formTerm.length === 0 && root.lookup.machinePack.offered && !root.lookup.machinePack.downloading
        tone: "primary"
        text: root.lookup.machinePack.bytes > 0
            ? qsTr("Download translation · %1 MB").arg(Math.round(root.lookup.machinePack.bytes / (1024 * 1024)))
            : qsTr("Download translation")

        onClicked: root.lookup.machinePack.download()
    }

    DesktopProgressBar
    {
        Layout.fillWidth: true
        visible: root.lookup.formTerm.length === 0 && root.lookup.machinePack.offered && root.lookup.machinePack.downloading
        fill: AppTheme.colourAccent
        progress: root.lookup.machinePack.progress
    }

    DesktopLabel
    {
        visible: root.lookup.formTerm.length === 0 && root.lookup.machinePack.offered && root.lookup.machinePack.downloading
        text: qsTr("Downloading translation, %1%").arg(Math.round(root.lookup.machinePack.progress * 100))
        color: AppTheme.colourMuted
    }
}
