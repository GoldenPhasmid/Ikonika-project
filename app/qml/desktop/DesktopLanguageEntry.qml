import QtQuick

QtObject
{
    id: root

    required property var entry
    required property var store

    readonly property bool active: root.entry.code === root.store.targetLanguageCode
    readonly property bool downloading: root.entry.id.length > 0
        && (AppPackageDownloader.activeId === root.entry.id
            || AppPackageDownloader.queue.indexOf(root.entry.id) >= 0)
    readonly property bool fetching: !root.entry.installed
        && (root.downloading
            || (AppOnboarding.chosenLanguage === root.entry.code
                && !(AppOnboarding.dictionaryReady && AppOnboarding.voiceReady)))
    readonly property bool actionable: root.entry.compatible && !root.fetching && !root.active

    readonly property real fetchProgress: AppPackageDownloader.totalBytes > 0
        ? AppPackageDownloader.receivedBytes / AppPackageDownloader.totalBytes
        : 0

    readonly property string status:
    {
        if (root.fetching)
        {
            return root.entry.outdated
                ? qsTr("Updating, %1%").arg(Math.round(root.fetchProgress * 100))
                : qsTr("Downloading, %1%").arg(Math.round(root.fetchProgress * 100))
        }

        if (!root.entry.compatible)
        {
            return root.entry.outdated
                ? qsTr("Update not available yet")
                : qsTr("Needs a newer app version")
        }

        if (!root.entry.installed)
        {
            return root.entry.outdated
                ? qsTr("Update, %1 MB").arg(root.megabytes(root.entry.bytes))
                : qsTr("Download, %1 MB").arg(root.megabytes(root.entry.bytes))
        }

        return ""
    }

    readonly property string actionLabel:
    {
        if (root.entry.installed)
        {
            return qsTr("Use")
        }

        return root.entry.outdated ? qsTr("Update") : qsTr("Download")
    }

    function megabytes(bytes)
    {
        return Math.round(bytes / (1024 * 1024))
    }

    function act()
    {
        if (!root.entry.compatible)
        {
            return
        }

        if (root.entry.installed)
        {
            root.store.selectTargetLanguage(root.entry.code)

            return
        }

        if (root.entry.outdated)
        {
            AppPackageDownloader.download(root.entry.id)

            return
        }

        AppOnboarding.choose(root.entry.code)
    }
}
