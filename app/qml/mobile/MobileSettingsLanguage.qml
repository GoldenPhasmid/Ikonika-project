import QtQuick
import QtQuick.Layouts

ColumnLayout
{
    id: root

    required property var store
    required property var languages
    required property var languageSheet
    required property var deleteLanguageSheet
    required property string languageCode

    signal placementRequested()

    readonly property var entry: AppCatalog.languages.find(function(language) { return language.code === root.languageCode }) || null
    readonly property string name: root.entry !== null ? root.entry.name : ""
    readonly property int words: root.languages.wordCount(root.languageCode)
    readonly property bool active: root.languageCode === root.store.targetLanguageCode
    readonly property bool installed: root.entry !== null && root.entry.installed

    readonly property bool fetching: root.entry !== null && !root.installed
        && ((root.entry.id.length > 0
             && (AppPackageDownloader.activeId === root.entry.id
                 || AppPackageDownloader.queue.indexOf(root.entry.id) >= 0))
            || (AppOnboarding.chosenLanguage === root.languageCode
                && !(AppOnboarding.dictionaryReady && AppOnboarding.voiceReady)))

    readonly property int fetchPercent: AppPackageDownloader.totalBytes > 0
        ? Math.round(AppPackageDownloader.receivedBytes / AppPackageDownloader.totalBytes * 100)
        : 0

    Layout.fillWidth: true
    spacing: AppMetrics.gapLarge

    MobileCard
    {
        spacing: AppMetrics.gapMedium

        RowLayout
        {
            Layout.fillWidth: true
            spacing: AppMetrics.gapMedium

            Rectangle
            {
                Layout.preferredWidth: 48
                Layout.preferredHeight: 48

                radius: AppMetrics.radiusMedium
                color: root.active ? AppTheme.colourText : "transparent"
                border.width: root.active ? 0 : AppMetrics.borderWidth
                border.color: AppTheme.colourBorder

                Text
                {
                    anchors.centerIn: parent

                    text: root.languageCode.toUpperCase()
                    color: root.active ? AppTheme.colourPanel : AppTheme.colourSubtle
                    font.pixelSize: AppMetrics.fontSmall
                    font.weight: Font.Bold
                    font.family: AppMetrics.fontFamily
                }
            }

            ColumnLayout
            {
                Layout.fillWidth: true
                spacing: 2

                Text
                {
                    Layout.fillWidth: true

                    text: root.name
                    color: AppTheme.colourText
                    font.pixelSize: AppMetrics.fontLabel
                    font.weight: Font.Medium
                    font.family: AppMetrics.fontFamily
                    elide: Text.ElideRight
                }

                Text
                {
                    Layout.fillWidth: true

                    text: root.words > 0 ? qsTr("%n words", "", root.words) : qsTr("No words yet")
                    color: AppTheme.colourMuted
                    font.pixelSize: AppMetrics.fontMeta
                    font.family: AppMetrics.fontFamily
                }
            }

            Text
            {
                visible: root.active

                text: qsTr("Learning now")
                color: AppTheme.colourAccent
                font.pixelSize: AppMetrics.fontMeta
                font.weight: Font.DemiBold
                font.family: AppMetrics.fontFamily
            }
        }

        MobileWideButton
        {
            objectName: "language_learn"

            visible: !root.active && root.entry !== null
            label:
            {
                if (root.entry === null)
                {
                    return ""
                }

                if (root.fetching)
                {
                    return qsTr("Downloading, %1%").arg(root.fetchPercent)
                }

                if (root.installed)
                {
                    return qsTr("Learn %1").arg(root.name)
                }

                return qsTr("Download · %1 MB").arg(Math.round(root.entry.bytes / (1024 * 1024)))
            }
            interactive: !root.fetching && root.entry !== null && root.entry.compatible

            onClicked: root.languageSheet.act(root.entry)
        }
    }

    MobileSettingsGroup
    {
        visible: root.active && (AppMachineTranslation.installed || AppMachineTranslation.offered)
        title: qsTr("SENTENCE TRANSLATION")

        MobileSettingsRow
        {
            objectName: "language_translation"

            title:
            {
                if (AppMachineTranslation.installed)
                {
                    return qsTr("Installed")
                }

                if (AppMachineTranslation.downloading)
                {
                    return qsTr("Downloading, %1%").arg(Math.round(AppMachineTranslation.progress * 100))
                }

                return AppMachineTranslation.bytes > 0
                    ? qsTr("Download · %1 MB").arg(Math.round(AppMachineTranslation.bytes / (1024 * 1024)))
                    : qsTr("Download")
            }
            lastRow: true

            onClicked: if (AppMachineTranslation.offered && !AppMachineTranslation.downloading) AppMachineTranslation.download()
        }
    }

    MobileSettingsGroup
    {
        visible: root.words > 0
        title: qsTr("REMOVE")

        MobileSettingsRow
        {
            objectName: "language_delete"

            title: qsTr("Delete %n words", "", root.words)
            destructive: true
            lastRow: true

            onClicked: root.deleteLanguageSheet.openFor(root.entry, root.words)

            MobileSettingsChevron {}
        }
    }
}
