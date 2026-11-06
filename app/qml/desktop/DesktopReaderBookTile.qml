import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../ReaderShelf.js" as Shelf

Button
{
    id: root

    property string title: ""
    property string format: ""
    property string level: ""
    property bool exists: true
    property bool local: true
    property bool inCloud: false
    property bool free: false
    property bool cloudAvailable: false
    property real progress: -1
    property real size: 0
    property real readProgress: -1
    property bool morphHidden: false
    property real heldProgress: -1

    readonly property Item face: cover
    readonly property string placeIcon: Shelf.place(root, "monitor")
    readonly property bool progressShown: (root.transferring && root.local) || (!root.transferring && root.readProgress >= 0)

    signal showRequested()
    signal removeRequested()
    signal uploadRequested()
    signal cloudDeleteRequested()
    signal cancelRequested()

    readonly property bool transferring: root.progress >= 0
    readonly property bool downloading: root.transferring && !root.local
    readonly property bool missing: root.local && !root.exists
    readonly property bool actionsShown: (root.hovered || root.visualFocus) && !root.transferring
        && (root.local || (root.inCloud && root.cloudAvailable))
    readonly property int percent: Math.round(Math.max(0, root.progress) * 100)
    readonly property color coverColour: root.missing ? AppTheme.colourSunken : Shelf.tint(root.title, AppTheme.coverColours)
    readonly property color inkColour: root.missing ? AppTheme.colourMuted : AppTheme.colourCoverInk
    readonly property string brief: Shelf.brief(root)

    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    padding: 0

    z: root.hovered ? 1 : 0
    scale: root.hovered && root.enabled && !root.transferring ? AppMetrics.coverHoverScale : 1.0

    Behavior on scale { NumberAnimation { duration: AppMetrics.motionBase; easing.type: Easing.OutCubic } }

    text: root.title
    Accessible.name: qsTr("%1, %2, %3 %4").arg(root.title).arg(root.format.toUpperCase())
                                          .arg(root.missing ? qsTr("File not found") : Shelf.placeText(root, qsTr("On this computer only")))
                                          .arg(root.brief) + (root.level.length > 0 ? ", " + Shelf.levelLabel(root.level) : "")

    Keys.onReturnPressed: root.clicked()
    Keys.onEnterPressed: root.clicked()
    Keys.onDeletePressed: if (root.local) root.removeRequested()

    implicitHeight: Math.round(root.width / AppMetrics.readerTileAspect)

    contentItem: Item
    {
        DesktopBookCover
        {
            id: cover

            anchors.fill: parent
            opacity: root.morphHidden ? 0 : (!root.local ? AppTheme.opacityDisabled : (root.down ? AppTheme.opacityHover : 1.0))

            title: root.title
            format: root.format
            placeIcon: root.placeIcon
            coverColour: root.coverColour
            inkColour: root.inkColour
            missing: root.missing
            highlighted: root.hovered
            progressShown: root.progressShown
            progress: root.transferring ? root.progress : (root.heldProgress >= 0 ? root.heldProgress : root.readProgress)
            progressAnimated: !root.transferring && !root.morphHidden
        }

        ReaderLevelBadge
        {
            id: levelBadge

            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: AppMetrics.gapMedium
            visible: root.level.length > 0 && !root.morphHidden && opacity > 0
            opacity: 1 - coverActions.opacity
            level: root.level
            textSize: AppMetrics.fontSmall
        }

        DesktopCoverBar
        {
            id: coverActions

            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: AppMetrics.gapMedium
            shown: !root.morphHidden && (root.actionsShown || (root.transferring && root.local))

            DesktopCoverButton
            {
                visible: root.local && root.exists && !root.transferring
                iconName: "external_link"
                text: qsTr("Show in folder")

                onClicked: root.showRequested()
            }

            DesktopCoverButton
            {
                objectName: "reader_book_upload"

                visible: root.local && root.exists && !root.inCloud && !root.free && root.cloudAvailable && !root.transferring
                iconName: "cloud_upload"
                text: qsTr("Upload to the cloud")

                onClicked: root.uploadRequested()
            }

            DesktopCoverButton
            {
                objectName: "reader_remove_book"

                visible: root.local && !root.transferring
                iconName: "trash"
                iconColour: AppTheme.colourWrong
                text: root.inCloud ? qsTr("Remove from this device") : qsTr("Remove from library")

                onClicked: root.removeRequested()
            }

            DesktopCoverButton
            {
                objectName: "reader_book_cloud_delete"

                visible: root.inCloud && root.cloudAvailable && !root.transferring
                iconName: "cloud_off"
                iconColour: AppTheme.colourWrong
                text: qsTr("Delete from the cloud")

                onClicked: root.cloudDeleteRequested()
            }

            DesktopCoverButton
            {
                objectName: "reader_book_cancel_upload"

                visible: root.local && root.transferring
                iconName: "close"
                text: qsTr("Cancel the upload")

                onClicked: root.cancelRequested()
            }
        }

        Rectangle
        {
            anchors.fill: parent
            visible: root.downloading
            radius: AppMetrics.coverRadius
            color: AppTheme.colourScrim

            ColumnLayout
            {
                anchors.centerIn: parent
                spacing: AppMetrics.gapTight

                DesktopLabel
                {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("%1%").arg(root.percent)
                    font.pixelSize: AppMetrics.fontHeadline
                    font.weight: Font.DemiBold
                    color: AppTheme.colourCoverInk
                }

                DesktopLabel
                {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("%1 of %2 MB").arg((root.size * Math.max(0, root.progress) / 1048576).toFixed(1))
                                             .arg((root.size / 1048576).toFixed(1))
                    font.pixelSize: AppMetrics.fontSmall
                    color: AppTheme.colourCoverInk
                }

                DesktopCoverBar
                {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: AppMetrics.gapSmall

                    DesktopCoverButton
                    {
                        objectName: "reader_book_cancel_download"

                        iconName: "close"
                        text: qsTr("Cancel the download")

                        onClicked: root.cancelRequested()
                    }
                }
            }
        }

        DesktopFocusRing
        {
            visible: root.visualFocus
            baseRadius: AppMetrics.coverRadius
        }
    }

    background: Item {}

    HoverHandler
    {
        cursorShape: root.transferring ? Qt.ArrowCursor : Qt.PointingHandCursor
    }
}
