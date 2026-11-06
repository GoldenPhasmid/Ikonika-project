import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

DesktopPopover
{
    id: root

    required property var store

    heading: qsTr("Voice")

    readonly property real fetchProgress: AppPackageDownloader.totalBytes > 0
        ? AppPackageDownloader.receivedBytes / AppPackageDownloader.totalBytes
        : 0

    function megabytes(bytes)
    {
        return Math.round(bytes / (1024 * 1024))
    }

    function act(entry)
    {
        if (entry.installed)
        {
            root.store.selectVoice(entry.index)
            root.store.playVoiceSample()
            root.close()

            return
        }

        root.store.fetchVoice(entry.packageId)
    }

    onAboutToShow: AppCatalog.Refresh()

    Repeater
    {
        model: root.store.voiceChoices

        delegate: DesktopPopoverRow
        {
            id: voiceRow

            required property var modelData

            readonly property bool fetching: !voiceRow.modelData.installed
                && (AppPackageDownloader.activeId === voiceRow.modelData.packageId
                    || AppPackageDownloader.queue.indexOf(voiceRow.modelData.packageId) >= 0)

            text: voiceRow.modelData.title
            trailingText: !voiceRow.modelData.installed && !voiceRow.fetching
                ? qsTr("%1 MB").arg(root.megabytes(voiceRow.modelData.bytes))
                : ""
            progress: voiceRow.fetching ? root.fetchProgress : -1
            showsCheck: true
            selected: voiceRow.modelData.selected
            enabled: !voiceRow.fetching

            onClicked: root.act(voiceRow.modelData)
        }
    }
}
