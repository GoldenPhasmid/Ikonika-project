import QtQuick
import QtQuick.Layouts

ColumnLayout
{
    id: root

    required property var store

    signal languageOpened(string code)
    signal placementRequested()

    readonly property real actionWidth: Math.max(actionMetrics.advanceWidth(qsTr("Download")),
        actionMetrics.advanceWidth(qsTr("Manage")), actionMetrics.advanceWidth(qsTr("Retake"))) + AppMetrics.gapMedium * 2

    FontMetrics
    {
        id: actionMetrics
        font.family: AppMetrics.fontFamily
        font.pixelSize: AppMetrics.fontLabel
        font.weight: Font.DemiBold
    }

    property var wordCounts: ({})

    readonly property var languagesWithWords: AppCatalog.languages.filter(Entry => (root.wordCounts[Entry.code] || 0) > 0)

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

        MobileSettingsRow
        {
            objectName: "account_current_language"
            title: qsTr("Dictionary")
            description: qsTr("%n words in My Vocabulary", "", root.wordCount(root.store.targetLanguageCode))
            onClicked: root.languageOpened(root.store.targetLanguageCode)

            DesktopButton
            {
                large: true
                implicitWidth: root.actionWidth
                text: qsTr("Manage")
                onClicked: root.languageOpened(root.store.targetLanguageCode)
            }
        }

        MobileSettingsRow
        {
            title: qsTr("Device voice")
            description: qsTr("Pronunciation is managed by Android")
        }

        MobileSettingsRow
        {
            objectName: "account_translation"
            title: qsTr("Offline translation")
            description: qsTr("%1 → %2").arg(root.store.targetLanguageName).arg(root.store.nativeLanguageName)
            onClicked: if (AppMachineTranslation.offered && !AppMachineTranslation.downloading) AppMachineTranslation.download()

            DesktopButton
            {
                objectName: "account_translation_download"
                large: true
                implicitWidth: root.actionWidth
                visible: AppMachineTranslation.offered && !AppMachineTranslation.downloading
                text: qsTr("Download")
                Accessible.name: qsTr("Download %1 translation").arg(AppMachineTranslation.pairLabel)
                onClicked: AppMachineTranslation.download()
            }

            Text
            {
                visible: !AppMachineTranslation.offered || AppMachineTranslation.downloading
                text: AppMachineTranslation.installed ? qsTr("Installed")
                    : AppMachineTranslation.downloading ? qsTr("Downloading…") : qsTr("Unavailable")
                font.family: AppMetrics.fontFamily
                font.pixelSize: AppMetrics.fontSmall
                color: AppTheme.colourMuted
            }
        }

        MobileSettingsRow
        {
            objectName: "account_placement_test"
            title: qsTr("Placement test")
            lastRow: true
            onClicked: root.placementRequested()

            DesktopButton
            {
                large: true
                implicitWidth: root.actionWidth
                text: qsTr("Retake")
                onClicked: root.placementRequested()
            }
        }
    }

    ColumnLayout
    {
        Layout.fillWidth: true
        spacing: AppMetrics.gapSmall

        MobileSectionLabel { text: qsTr("Other languages") }

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

        GridLayout
        {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: AppMetrics.gapSmall
            rowSpacing: AppMetrics.gapSmall

            Repeater
            {
                model: AppCatalog.languages.filter(entry => entry.code !== root.store.targetLanguageCode)

                delegate: DesktopLanguageTile
                {
                    required property var modelData
                    objectName: "account_language_" + modelData.code
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    entry: modelData
                    store: root.store
                    words: root.wordCount(modelData.code)
                }
            }
        }
    }
}
