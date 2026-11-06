import QtQuick
import QtQuick.Layouts

MobileBottomSheet
{
    id: root

    readonly property var update: AppCatalog.availableDictionaryUpdate

    readonly property string packageId: root.update.id !== undefined ? root.update.id : ""

    readonly property bool downloading: root.packageId.length > 0
        && (AppPackageDownloader.activeId === root.packageId
            || AppPackageDownloader.queue.indexOf(root.packageId) >= 0)

    readonly property real fetchProgress: AppPackageDownloader.totalBytes > 0
        ? AppPackageDownloader.receivedBytes / AppPackageDownloader.totalBytes
        : 0

    heading: qsTr("Update the %1 dictionary").arg(root.update.name !== undefined
        ? root.update.name
        : "")
    showClose: !root.downloading

    visible: false

    onDismissed: root.close()

    onPackageIdChanged:
    {
        if (root.packageId.length === 0)
        {
            root.close()
        }
    }

    function megabytes(bytes)
    {
        return Math.round(bytes / (1024 * 1024))
    }

    function offer()
    {
        if (root.packageId.length === 0)
        {
            return
        }

        root.present()
    }

    function close()
    {
        root.hide()
    }

    function later()
    {
        AppCatalog.dismissDictionaryUpdate()
        root.close()
    }

    function start()
    {
        AppPackageDownloader.download(root.packageId)
    }

    Connections
    {
        target: AppPackageDownloader

        function onPackageInstalled(id)
        {
            if (id === root.packageId || root.downloading)
            {
                root.close()
            }
        }
    }

    Text
    {
        Layout.fillWidth: true

        text: qsTr("A newer dictionary is available, %1 MB to download. Your cards "
            + "and progress are not affected.")
            .arg(root.megabytes(root.update.bytes !== undefined ? root.update.bytes : 0))
        color: AppTheme.colourSubtle
        font.pixelSize: AppMetrics.fontBody
        font.family: AppMetrics.fontFamily
        wrapMode: Text.WordWrap
    }

    ColumnLayout
    {
        Layout.fillWidth: true
        visible: root.downloading
        spacing: AppMetrics.gapSmall

        Rectangle
        {
            Layout.fillWidth: true
            Layout.preferredHeight: 6

            radius: 3
            color: AppTheme.colourBorder

            Rectangle
            {
                width: parent.width * root.fetchProgress
                height: parent.height

                radius: parent.radius
                color: AppTheme.colourAccent
            }
        }

        Text
        {
            Layout.fillWidth: true

            text: qsTr("%1 of %2 MB")
                .arg(root.megabytes(AppPackageDownloader.receivedBytes))
                .arg(root.megabytes(AppPackageDownloader.totalBytes))
            color: AppTheme.colourMuted
            font.pixelSize: AppMetrics.fontMeta
            font.family: AppMetrics.fontFamily
        }
    }

    Text
    {
        Layout.fillWidth: true
        visible: AppPackageDownloader.lastError.length > 0

        text: AppPackageDownloader.lastError
        color: AppTheme.colourWrong
        font.pixelSize: AppMetrics.fontMeta
        font.family: AppMetrics.fontFamily
        wrapMode: Text.WordWrap
    }

    MobileButtonPair
    {
        Layout.fillWidth: true
        visible: !root.downloading

        leftLabel: qsTr("Not now")
        rightLabel: qsTr("Update")

        onLeftClicked: root.later()
        onRightClicked: root.start()
    }
}
