import QtQuick
import QtQuick.Layouts

DesktopSettingsGroup
{
    id: root

    required property var store
    property int words: 0
    property var triage: null
    signal placementRequested()
    signal deleteRequested()

    title: qsTr("I am learning")

    Item
    {
        objectName: "account_active_language"

        Layout.fillWidth: true
        implicitHeight: activeRow.implicitHeight + AppMetrics.gapLarge * 2

        RowLayout
        {
            id: activeRow

            anchors.fill: parent
            anchors.margins: AppMetrics.gapLarge
            spacing: AppMetrics.gapMedium

            LanguageFlag
            {
                code: root.store.targetLanguageCode
            }

            ColumnLayout
            {
                Layout.fillWidth: true
                spacing: AppMetrics.gapTight

                DesktopLabel
                {
                    Layout.fillWidth: true
                    text: root.store.targetLanguageName
                    font.pixelSize: AppMetrics.fontTitle
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                DesktopLabel
                {
                    Layout.fillWidth: true
                    text: qsTr("%n words in My Vocabulary", "", root.words)
                    font.pixelSize: AppMetrics.fontSmall
                    color: AppTheme.colourMuted
                    elide: Text.ElideRight
                }
            }
        }

        DesktopDivider
        {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: AppMetrics.gapLarge
            anchors.rightMargin: AppMetrics.gapLarge
        }
    }

    DesktopSettingsRow
    {
        text: qsTr("Dictionary")
        description: qsTr("%1 words in the dictionary").arg(Number(root.store.dictionaryWordCount).toLocaleString(Qt.locale(), "f", 0))

        DesktopLabel
        {
            text: qsTr("Installed")
            color: AppTheme.colourMuted
        }
    }

    DesktopSettingsRow
    {
        objectName: "account_translation"
        text: qsTr("Offline translation")
        description: qsTr("%1 → %2").arg(root.store.targetLanguageName).arg(root.store.nativeLanguageName)

        DesktopLabel
        {
            visible: !AppMachineTranslation.offered || AppMachineTranslation.downloading
            text: AppMachineTranslation.installed ? qsTr("Installed")
                : AppMachineTranslation.downloading ? qsTr("Downloading, %1%").arg(Math.round(AppMachineTranslation.progress * 100))
                : qsTr("Not available for this language pair")
            color: AppTheme.colourMuted
        }

        DesktopButton
        {
            objectName: "account_translation_download"
            Layout.preferredWidth: AppMetrics.settingsControlWidth
            visible: AppMachineTranslation.offered && !AppMachineTranslation.downloading
            text: qsTr("Download · %1 MB").arg(Math.round(AppMachineTranslation.bytes / (1024 * 1024)))
            Accessible.name: qsTr("Download %1 translation").arg(AppMachineTranslation.pairLabel)
            onClicked: AppMachineTranslation.download()
        }
    }

    DesktopRankBar
    {
        objectName: "account_word_range"

        visible: !!root.triage && root.triage.maximumRank > 0
        title: qsTr("Training offers new words after this rank")
        rank: root.triage ? root.triage.startRank : 0
        maximumRank: root.triage ? root.triage.maximumRank : 0
        levelMarks: root.triage ? root.triage.levelMarks : []

        onVisibleChanged: if (visible && root.triage) root.triage.refreshStartRank()
        onRankChosen: function(rank) { root.triage.startRank = rank }
    }

    DesktopSettingsRow
    {
        objectName: "account_placement_test"
        text: qsTr("Placement test")
        lastRow: !deleteRow.visible

        DesktopButton
        {
            objectName: "placement_retake"

            Layout.preferredWidth: AppMetrics.settingsControlWidth
            text: qsTr("Retake")
            onClicked: root.placementRequested()
        }
    }

    DesktopSettingsRow
    {
        id: deleteRow

        visible: root.words > 0
        text: qsTr("Delete %1 words").arg(root.store.targetLanguageName)
        description: qsTr("Words and learning progress")
        lastRow: true

        DesktopButton
        {
            objectName: "account_delete_" + root.store.targetLanguageCode
            Layout.preferredWidth: AppMetrics.settingsControlWidth
            tone: "dangerOutline"
            text: qsTr("Delete")
            onClicked: root.deleteRequested()
        }
    }
}
