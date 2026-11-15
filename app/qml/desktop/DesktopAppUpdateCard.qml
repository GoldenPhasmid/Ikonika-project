import QtQuick
import QtQuick.Layouts

DesktopUpdateCard
{
    id: root

    readonly property DesktopAppDownload status: DesktopAppDownload {}

    visible: root.status.available
    title: root.status.title
    detail: root.status.detail
    error: root.status.error
    progress: root.status.progress

    DesktopButton
    {
        Layout.fillWidth: true
        objectName: "strip_app_update_apply"

        visible: AppUpdater.ready
        enabled: !root.status.packagesPending
        text: root.status.packagesPending ? qsTr("Finish downloads first") : qsTr("Restart and update")
        tone: "primary"

        onClicked: root.status.start()
    }

    DesktopButton
    {
        Layout.fillWidth: true
        objectName: "strip_app_update_download"

        visible: !AppUpdater.ready && !AppUpdater.busy
        text: root.status.failed ? qsTr("Try again") : qsTr("Download")

        onClicked: root.status.start()
    }
}
