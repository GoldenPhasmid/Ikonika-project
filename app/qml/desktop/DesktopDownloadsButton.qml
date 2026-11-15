import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Shapes

DesktopIconButton
{
    id: root

    readonly property DesktopAppDownload appDownload: DesktopAppDownload {}
    readonly property DesktopPackageDownload firstPackage: DesktopPackageDownload
    {
        update: AppCatalog.packageUpdates.length > 0 ? AppCatalog.packageUpdates[0] : ({})
    }

    readonly property int count: (root.appDownload.available ? 1 : 0) + AppCatalog.packageUpdates.length
    readonly property DesktopDownloadStatus single: root.count !== 1
        ? null
        : (root.appDownload.available ? root.appDownload : root.firstPackage)

    readonly property real progress: AppUpdater.busy
        ? root.appDownload.progress
        : (AppPackageDownloader.busy
           ? (AppPackageDownloader.totalBytes > 0 ? AppPackageDownloader.receivedBytes / AppPackageDownloader.totalBytes : 0)
           : -1)

    function describe(status)
    {
        return [status.title, status.detail, status.error].filter(Line => Line.length > 0).join("\n")
    }

    visible: root.count > 0
    iconName: "arrow_down"
    iconColour: AppTheme.colourAccent
    checked: downloadList.visible
    text: root.single ? root.describe(root.single) : qsTr("%1 updates available").arg(root.count)

    ToolTip.visible: root.hovered && !downloadList.visible

    onCountChanged:
    {
        if (root.count === 0)
        {
            downloadList.close()
        }
    }

    onClicked:
    {
        if (root.single)
        {
            root.single.start()

            return
        }

        downloadList.visible ? downloadList.close() : downloadList.open()
    }

    Shape
    {
        anchors.fill: parent
        anchors.margins: AppMetrics.gapTight
        visible: root.progress >= 0
        preferredRendererType: Shape.CurveRenderer

        ShapePath
        {
            strokeColor: AppTheme.faded(AppTheme.colourAccent, AppTheme.alphaHalf)
            strokeWidth: AppMetrics.progressRingStroke / 2
            fillColor: "transparent"

            PathAngleArc
            {
                centerX: (root.width - AppMetrics.gapTight * 2) / 2
                centerY: (root.height - AppMetrics.gapTight * 2) / 2
                radiusX: centerX - AppMetrics.progressRingStroke / 2
                radiusY: radiusX
                startAngle: -90
                sweepAngle: 360
                moveToStart: true
            }
        }

        ShapePath
        {
            strokeColor: AppTheme.colourAccent
            strokeWidth: AppMetrics.progressRingStroke / 2
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap

            PathAngleArc
            {
                centerX: (root.width - AppMetrics.gapTight * 2) / 2
                centerY: (root.height - AppMetrics.gapTight * 2) / 2
                radiusX: centerX - AppMetrics.progressRingStroke / 2
                radiusY: radiusX
                startAngle: -90
                sweepAngle: 360 * Math.max(root.progress, 0)
                moveToStart: true
            }
        }
    }

    DesktopPopover
    {
        id: downloadList

        objectName: "downloads_list"

        x: root.width - width
        y: root.height + AppMetrics.gapTight
        heading: qsTr("Updates")

        DesktopPopoverRow
        {
            objectName: "download_app"

            visible: root.appDownload.available
            text: root.appDownload.name
            trailingText: root.appDownload.stateText

            onClicked: root.appDownload.start()
        }

        Repeater
        {
            model: AppCatalog.packageUpdates

            delegate: DesktopPopoverRow
            {
                id: packageRow

                required property var modelData

                readonly property DesktopPackageDownload status: DesktopPackageDownload { update: packageRow.modelData }

                objectName: "download_" + packageRow.status.updateId

                text: packageRow.status.name
                trailingText: packageRow.status.stateText

                onClicked: packageRow.status.start()
            }
        }

        DesktopLabel
        {
            Layout.fillWidth: true
            Layout.margins: AppMetrics.gapCompact
            visible: AppPackageDownloader.lastError.length > 0
            text: AppPackageDownloader.lastError
            font.pixelSize: AppMetrics.fontSmall
            color: AppTheme.colourWrong
            wrapMode: Text.WordWrap
        }
    }
}
