import QtQuick
import QtQuick.Layouts

ColumnLayout
{
    id: root

    required property var store
    property var triage: null

    signal languageOpened(string code)
    signal placementRequested()

    property var wordCounts: ({})

    readonly property var languagesWithWords: AppCatalog.languages.filter(Entry => (root.wordCounts[Entry.code] || 0) > 0)
    readonly property var otherLanguages: AppCatalog.languages.filter(entry => entry.code !== root.store.targetLanguageCode)

    function refresh()
    {
        const counts = {}

        for (const entry of AppCatalog.languages)
        {
            counts[entry.code] = AppSyncController.countLanguageWords(entry.code)
        }

        root.wordCounts = counts
    }

    function wordCount(code)
    {
        return root.wordCounts[code] || 0
    }

    Layout.fillWidth: true
    spacing: AppMetrics.gapLarge

    onVisibleChanged:
    {
        if (!root.visible)
        {
            return
        }

        if (AppCatalog.languages.length === 0)
        {
            AppCatalog.Refresh()
        }

        root.refresh()
    }

    Connections
    {
        target: AppCatalog

        function onCatalogChanged()
        {
            root.refresh()
        }
    }

    Connections
    {
        target: AppSyncController

        function onLocalDataChanged()
        {
            root.refresh()
        }
    }

    MobileSettingsGroup
    {
        title: qsTr("Learning %1").arg(root.store.targetLanguageName)
        flagCode: root.store.targetLanguageCode

        MobileSettingsRow
        {
            objectName: "account_current_language"
            title: qsTr("Dictionary")
            description: qsTr("%n words in My Vocabulary", "", root.wordCount(root.store.targetLanguageCode))
            onClicked: root.languageOpened(root.store.targetLanguageCode)

            MobileSettingsChevron {}
        }

        MobileSettingsRow
        {
            objectName: "account_translation"
            title: qsTr("Offline translation")
            description: AppMachineTranslation.downloading
                ? qsTr("Downloading, %1%").arg(Math.round(AppMachineTranslation.progress * 100))
                : qsTr("%1 → %2").arg(root.store.targetLanguageName).arg(root.store.nativeLanguageName)
            onClicked: if (AppMachineTranslation.offered && !AppMachineTranslation.downloading) AppMachineTranslation.download()

            DesktopIcon
            {
                objectName: "account_translation_download"
                visible: AppMachineTranslation.offered && !AppMachineTranslation.downloading
                name: "arrow_down"
                width: AppMetrics.iconSizeSmall
                height: width
                color: AppTheme.colourSubtle
                Accessible.name: qsTr("Download %1 translation").arg(AppMachineTranslation.pairLabel)
            }

            Text
            {
                visible: !AppMachineTranslation.offered && !AppMachineTranslation.downloading
                text: AppMachineTranslation.installed ? qsTr("Installed") : qsTr("Unavailable")
                font.family: AppMetrics.fontFamily
                font.pixelSize: AppMetrics.fontSmall
                color: AppTheme.colourMuted
            }
        }

        MobileRankBar
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

        MobileSettingsRow
        {
            objectName: "account_placement_test"
            title: qsTr("Placement test")
            lastRow: true
            onClicked: root.placementRequested()

            MobileSettingsChevron {}
        }
    }

    Text
    {
        Layout.fillWidth: true
        visible: AppCatalog.languages.length === 0
        text: AppCatalog.busy ? qsTr("Looking for languages…") : qsTr("Ikonika cannot reach the language list.")
        color: AppTheme.colourMuted
        font.pixelSize: AppMetrics.fontSmall
        font.family: AppMetrics.fontFamily
        wrapMode: Text.WordWrap
    }

    MobileSettingsGroup
    {
        visible: root.otherLanguages.length > 0
        title: qsTr("Other languages")

        Repeater
        {
            model: root.otherLanguages

            delegate: MobileSettingsRow
            {
                id: languageRow

                required property var modelData
                required property int index

                readonly property DesktopLanguageEntry language: DesktopLanguageEntry
                {
                    entry: languageRow.modelData
                    store: root.store
                }

                objectName: "account_language_" + languageRow.modelData.code
                title: languageRow.modelData.name
                description: languageRow.language.status.length > 0
                    ? languageRow.language.status
                    : qsTr("Switch · %n words", "", root.wordCount(languageRow.modelData.code))
                lastRow: languageRow.index === root.otherLanguages.length - 1

                onClicked: if (languageRow.language.actionable) languageRow.language.act()

                leading: LanguageFlag
                {
                    code: languageRow.modelData.code
                }

                DesktopIcon
                {
                    visible: languageRow.language.actionable
                    name: languageRow.modelData.installed ? "chevron_right" : "arrow_down"
                    width: AppMetrics.iconSizeSmall
                    height: width
                    color: AppTheme.colourSubtle
                }
            }
        }
    }
}
