import QtQuick

DesktopDownloadStatus
{
    id: root

    readonly property bool failed: AppUpdater.lastError.length > 0 && !AppUpdater.busy && !AppUpdater.ready
    readonly property bool packagesPending: AppPackageDownloader.busy || AppPackageDownloader.queue.length > 0

    function heading()
    {
        if (AppUpdater.ready)
        {
            return qsTr("Version %1 is ready to install").arg(AppCatalog.publishedVersion)
        }

        if (AppUpdater.busy)
        {
            return qsTr("Downloading version %1").arg(AppCatalog.publishedVersion)
        }

        if (root.failed)
        {
            return qsTr("Version %1 could not be downloaded").arg(AppCatalog.publishedVersion)
        }

        return qsTr("Version %1 is available").arg(AppCatalog.publishedVersion)
    }

    function start()
    {
        if (AppUpdater.ready)
        {
            if (root.packagesPending)
            {
                console.info("App update install waits for package downloads")

                return
            }

            console.info("Restarting to install version", AppCatalog.publishedVersion)
            AppUpdater.ApplyAndRestart()

            return
        }

        if (AppUpdater.busy)
        {
            return
        }

        console.info("Downloading app version", AppCatalog.publishedVersion)
        AppUpdater.Download()
    }

    function describeState()
    {
        if (AppUpdater.busy)
        {
            return root.percentText
        }

        if (AppUpdater.ready)
        {
            return root.packagesPending ? qsTr("Waiting") : qsTr("Restart")
        }

        if (root.failed)
        {
            return qsTr("Retry")
        }

        return root.sizeText
    }

    available: AppCatalog.updateAvailable && !AppUpdater.storePackage
    name: qsTr("Ikonika %1").arg(AppCatalog.publishedVersion)
    title: root.heading()
    stateText: root.describeState()
    sizeText: AppCatalog.publishedBytes > 0 ? qsTr("%1 MB").arg(root.megabytes(AppCatalog.publishedBytes)) : ""
    detail: AppUpdater.busy
        ? qsTr("%1 of %2 MB").arg(root.megabytes(AppUpdater.receivedBytes)).arg(root.megabytes(AppUpdater.totalBytes))
        : ""
    error: root.failed ? AppUpdater.lastError : ""
    progress: AppUpdater.busy
        ? (AppUpdater.totalBytes > 0 ? AppUpdater.receivedBytes / AppUpdater.totalBytes : 0)
        : -1
}
