import QtQuick
import QtQuick.Layouts

DesktopUpdateCard
{
    id: root

    required property var update
    readonly property string updateId: root.update.id !== undefined ? root.update.id : ""
    readonly property bool updating: root.updateId.length > 0
        && (AppPackageDownloader.activeId === root.updateId || AppPackageDownloader.queue.indexOf(root.updateId) >= 0)
    readonly property bool active: AppPackageDownloader.activeId === root.updateId

    function megabytes(bytes)
    {
        return Math.round(bytes / (1024 * 1024))
    }

    visible: root.updateId.length > 0 && (!root.dismissible || !root.update.dismissed)
    title: qsTr("Update %1").arg(root.update.name !== undefined ? root.update.name : "")
    onDismissed: AppCatalog.dismissPackageUpdate(root.updateId)
    detail: root.active
        ? qsTr("%1 of %2 MB").arg(root.megabytes(AppPackageDownloader.receivedBytes))
                             .arg(root.megabytes(AppPackageDownloader.totalBytes))
        : (root.updating ? qsTr("Queued") : "")
    progress: root.active
        ? (AppPackageDownloader.totalBytes > 0 ? AppPackageDownloader.receivedBytes / AppPackageDownloader.totalBytes : 0)
        : -1
    error: AppPackageDownloader.lastError

    DesktopButton
    {
        Layout.fillWidth: true
        objectName: "download_" + root.updateId

        visible: !root.updating
        text: qsTr("Update · %1 MB").arg(root.megabytes(root.update.bytes !== undefined ? root.update.bytes : 0))

        onClicked: AppPackageDownloader.download(root.updateId)
    }
}
