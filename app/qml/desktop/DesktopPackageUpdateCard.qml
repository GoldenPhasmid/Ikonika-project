import QtQuick
import QtQuick.Layouts

DesktopUpdateCard
{
    id: root

    required property var update

    readonly property DesktopPackageDownload status: DesktopPackageDownload { update: root.update }

    visible: root.status.available
    title: root.status.title
    detail: root.status.detail
    error: root.status.error
    progress: root.status.progress

    DesktopButton
    {
        Layout.fillWidth: true
        objectName: "download_" + root.status.updateId

        visible: !root.status.updating
        text: qsTr("Update · %1").arg(root.status.sizeText)

        onClicked: root.status.start()
    }
}
