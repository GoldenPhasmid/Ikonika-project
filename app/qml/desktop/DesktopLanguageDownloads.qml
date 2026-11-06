import QtQuick
import QtQuick.Layouts

DesktopSettingsGroup
{
    id: root

    required property var store
    property int words: 0
    signal placementRequested()

    readonly property var voice: AppCatalog.packages.find(entry => entry.kind === "voice"
        && entry.target === root.store.targetLanguageCode && entry.id.startsWith("piper-") && entry.compatible) || null
    readonly property bool voicePending: root.voice !== null
        && (AppPackageDownloader.activeId === root.voice.id || AppPackageDownloader.queue.indexOf(root.voice.id) >= 0)
    readonly property bool voiceActive: root.voice !== null && AppPackageDownloader.activeId === root.voice.id

    title: qsTr("Learning %1").arg(root.store.targetLanguageName)

    DesktopSettingsRow
    {
        text: qsTr("Dictionary")
        description: qsTr("%n words in My Vocabulary", "", root.words)

        DesktopLabel
        {
            text: qsTr("Installed")
            color: AppTheme.colourMuted
        }
    }

    DesktopSettingsRow
    {
        objectName: "account_piper_voice"
        text: qsTr("Piper voice")
        description: qsTr("Offline pronunciation")

        DesktopLabel
        {
            visible: root.voice === null || root.voice.installed || root.voicePending
            text: root.voice === null ? qsTr("Not available for this language")
                : root.voice.installed ? qsTr("Installed")
                : !root.voiceActive ? qsTr("Queued")
                : qsTr("Downloading, %1%").arg(Math.round(AppPackageDownloader.totalBytes > 0
                    ? AppPackageDownloader.receivedBytes / AppPackageDownloader.totalBytes * 100 : 0))
            color: AppTheme.colourMuted
        }

        DesktopButton
        {
            objectName: "account_piper_download"
            Layout.preferredWidth: AppMetrics.settingsControlWidth
            visible: root.voice !== null && !root.voice.installed && !root.voicePending
            text: qsTr("Download · %1 MB").arg(root.voice ? Math.round(root.voice.bytes / (1024 * 1024)) : 0)
            Accessible.name: qsTr("Download %1 Piper voice").arg(root.store.targetLanguageName)
            onClicked: AppPackageDownloader.download(root.voice.id)
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

    DesktopSettingsRow
    {
        objectName: "account_placement_test"
        text: qsTr("Placement test")
        lastRow: true

        DesktopButton
        {
            Layout.preferredWidth: AppMetrics.settingsControlWidth
            text: qsTr("Retake")
            onClicked: root.placementRequested()
        }
    }
}
