import QtQuick

DesktopDownloadStatus
{
    id: root

    required property var update

    readonly property string updateId: root.update.id !== undefined ? root.update.id : ""
    readonly property bool active: root.updateId.length > 0 && AppPackageDownloader.activeId === root.updateId
    readonly property bool updating: root.active
        || (root.updateId.length > 0 && AppPackageDownloader.queue.indexOf(root.updateId) >= 0)

    function start()
    {
        if (root.updateId.length === 0 || root.updating)
        {
            return
        }

        console.info("Downloading package update", root.updateId)
        AppPackageDownloader.download(root.updateId)
    }

    available: root.updateId.length > 0
    name: root.update.name !== undefined ? root.update.name : ""
    title: qsTr("Update %1").arg(root.name)
    stateText: root.active ? root.percentText : (root.updating ? qsTr("Queued") : root.sizeText)
    detail: root.active
        ? qsTr("%1 of %2 MB").arg(root.megabytes(AppPackageDownloader.receivedBytes))
                             .arg(root.megabytes(AppPackageDownloader.totalBytes))
        : (root.updating ? qsTr("Queued") : "")
    error: AppPackageDownloader.lastError
    sizeText: qsTr("%1 MB").arg(root.megabytes(root.update.bytes !== undefined ? root.update.bytes : 0))
    progress: root.active
        ? (AppPackageDownloader.totalBytes > 0 ? AppPackageDownloader.receivedBytes / AppPackageDownloader.totalBytes : 0)
        : -1
}
