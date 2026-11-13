import QtQuick
import QtQuick.Layouts

DesktopUpdateCard
{
    id: root

    readonly property bool failed: AppUpdater.lastError.length > 0 && !AppUpdater.busy && !AppUpdater.ready
    readonly property bool packagesPending: AppPackageDownloader.busy || AppPackageDownloader.queue.length > 0

    function megabytes(bytes)
    {
        return Math.round(bytes / (1024 * 1024))
    }

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

    visible: AppCatalog.updateAvailable && !AppUpdater.storePackage
        && (!root.dismissible || !AppCatalog.appUpdateDismissed)
    onDismissed: AppCatalog.dismissAppUpdate()
    title: root.heading()
    detail: AppUpdater.busy
        ? qsTr("%1 of %2 MB").arg(root.megabytes(AppUpdater.receivedBytes)).arg(root.megabytes(AppUpdater.totalBytes))
        : (root.failed ? AppUpdater.lastError : "")
    progress: AppUpdater.busy
        ? (AppUpdater.totalBytes > 0 ? AppUpdater.receivedBytes / AppUpdater.totalBytes : 0)
        : -1

    DesktopButton
    {
        Layout.fillWidth: true
        objectName: "strip_app_update_apply"

        visible: AppUpdater.ready
        enabled: !root.packagesPending
        text: root.packagesPending ? qsTr("Finish downloads first") : qsTr("Restart and update")
        tone: "primary"

        onClicked: AppUpdater.ApplyAndRestart()
    }

    DesktopButton
    {
        Layout.fillWidth: true
        objectName: "strip_app_update_download"

        visible: !AppUpdater.ready && !AppUpdater.busy
        text: root.failed ? qsTr("Try again") : qsTr("Download")

        onClicked: AppUpdater.Download()
    }
}
