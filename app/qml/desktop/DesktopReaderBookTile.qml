import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../ReaderShelf.js" as Shelf

Button
{
    id: root

    property string title: ""
    property string author: ""
    property string format: ""
    property string level: ""
    property string coverSource: ""
    property bool exists: true
    property bool local: true
    property bool free: false
    property bool cloudAvailable: false
    property bool favorite: false
    property bool finished: false
    property real progress: -1
    property real size: 0
    property real readProgress: -1

    readonly property bool progressShown: !root.transferring && root.readProgress > 0 && !root.finished
    readonly property bool transferring: root.progress >= 0
    readonly property bool downloading: root.transferring && !root.local
    readonly property bool uploading: root.transferring && root.local
    readonly property bool missing: root.local && !root.exists
    readonly property bool onlyInCloud: !root.local && !root.transferring
    readonly property bool downloadable: root.onlyInCloud && (root.free || root.cloudAvailable)
    readonly property bool menuOffered: root.local && !root.transferring
    readonly property bool actionsShown: root.hovered && root.transferring
    property bool actionsFading: false
    readonly property int percent: Math.round(Math.max(0, root.progress) * 100)
    readonly property int readPercent: Math.ceil(Math.max(0, root.readProgress) * 100)

    signal removeRequested()
    signal overviewRequested()
    signal cancelRequested()

    component CoverAction: DesktopCoverButton
    {
        id: action

        implicitWidth: AppMetrics.bookActionSize
        implicitHeight: AppMetrics.bookActionSize
        iconColour: AppTheme.colourCoverInk

        background: Rectangle
        {
            radius: width / 2
            color: AppTheme.faded(AppTheme.colourScrim, action.hovered ? AppTheme.opacityHover : AppTheme.opacitySecondary)
        }
    }

    onActionsShownChanged:
    {
        if (root.actionsShown)
        {
            actionsFadeTimer.stop()
            root.actionsFading = true

            return
        }

        actionsFadeTimer.restart()
    }

    Timer
    {
        id: actionsFadeTimer

        interval: AppMetrics.motionFast

        onTriggered: root.actionsFading = false
    }

    function openOverview()
    {
        if (root.menuOffered)
        {
            root.overviewRequested()
        }
    }

    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    padding: 0

    text: root.title
    Accessible.name: [root.title, root.author, root.missing ? qsTr("File not found")
                                                            : root.onlyInCloud ? qsTr("Not downloaded") : "",
                      root.finished ? qsTr("Finished") : root.readPercent > 0 ? qsTr("%1% read").arg(root.readPercent) : "",
                      root.favorite ? qsTr("Favorite") : "", Shelf.levelLabel(root.level)]
                         .filter(function(part) { return part.length > 0 }).join(", ")

    Keys.onReturnPressed: root.clicked()
    Keys.onEnterPressed: root.clicked()
    Keys.onDeletePressed: if (root.local) root.removeRequested()
    Keys.onMenuPressed: root.openOverview()

    implicitHeight: content.implicitHeight

    contentItem: ColumnLayout
    {
        id: content

        spacing: 0

        Item
        {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.round(width * AppMetrics.bookCoverAspect)

            DesktopBookCover
            {
                id: cover

                width: parent.width
                height: parent.height
                opacity: root.down ? AppTheme.opacityHover : 1.0

                title: root.title
                author: root.author
                imageSource: root.coverSource
                missing: root.missing
                progressShown: root.progressShown
                progress: root.readProgress
                progressAnimated: true
            }

            Rectangle
            {
                anchors.fill: cover
                radius: AppMetrics.bookRadius
                color: AppTheme.faded(AppTheme.colourCoverInk, AppTheme.alphaSoft)
                opacity: root.hovered && !root.transferring ? 1 : 0
                visible: opacity > 0

                Behavior on opacity { NumberAnimation { duration: AppMetrics.motionFast } }
            }

            Rectangle
            {
                anchors.fill: cover
                anchors.margins: -AppMetrics.gapTight
                radius: AppMetrics.bookRadius + AppMetrics.gapTight
                color: "transparent"
                border.width: AppMetrics.focusRingWidth
                border.color: AppTheme.colourOutline
                opacity: root.hovered && !root.transferring && !root.visualFocus ? 1 : 0
                visible: opacity > 0

                Behavior on opacity { NumberAnimation { duration: AppMetrics.motionFast } }
            }

            Rectangle
            {
                objectName: "reader_book_download"

                anchors.right: cover.right
                anchors.bottom: cover.bottom
                anchors.margins: AppMetrics.gapMedium
                width: AppMetrics.bookRingSize
                height: AppMetrics.bookRingSize
                radius: width / 2
                visible: root.downloadable && !root.transferring
                color: AppTheme.faded(AppTheme.colourScrim, root.hovered ? AppTheme.opacityHover : AppTheme.opacitySecondary)

                Behavior on color { ColorAnimation { duration: AppMetrics.motionFast } }

                DesktopIcon
                {
                    anchors.centerIn: parent
                    name: "download"
                    color: AppTheme.colourCoverInk
                    width: Math.round(AppMetrics.bookRingSize / 2)
                    height: Math.round(AppMetrics.bookRingSize / 2)
                }
            }

            Loader
            {
                anchors.fill: cover
                active: root.transferring

                sourceComponent: Component
                {
                    Rectangle
                    {
                        radius: AppMetrics.bookRadius
                        color: AppTheme.faded(AppTheme.colourScrim, AppTheme.opacitySecondary)

                        DesktopGoalRing
                        {
                            anchors.centerIn: parent
                            width: AppMetrics.bookRingSize
                            height: AppMetrics.bookRingSize
                            progress: Math.max(0, root.progress)
                            stroke: AppMetrics.bookRingStroke
                            fill: AppTheme.colourCoverInk
                            track: AppTheme.faded(AppTheme.colourCoverInk, AppTheme.alphaStrong)
                            sweepDuration: AppMetrics.motionFast

                            DesktopLabel
                            {
                                text: qsTr("%1%").arg(root.percent)
                                font.pixelSize: AppMetrics.fontSmall
                                font.weight: Font.DemiBold
                                color: AppTheme.colourCoverInk
                            }
                        }
                    }
                }
            }

            Loader
            {
                id: actions

                anchors.right: cover.right
                anchors.top: cover.top
                anchors.margins: AppMetrics.gapSmall
                active: root.actionsShown || root.actionsFading

                sourceComponent: Component
                {
                    Row
                    {
                        spacing: AppMetrics.gapSmall
                        opacity: root.actionsShown ? 1 : 0

                        Behavior on opacity { NumberAnimation { duration: AppMetrics.motionFast } }

                        CoverAction
                        {
                            objectName: "reader_book_cancel"

                            iconName: "close"
                            text: root.uploading ? qsTr("Cancel the upload") : qsTr("Cancel the download")

                            onClicked: root.cancelRequested()
                        }
                    }
                }
            }

            DesktopFocusRing
            {
                anchors.fill: cover
                visible: root.visualFocus
                baseRadius: AppMetrics.bookRadius
            }
        }

        DesktopLabel
        {
            Layout.fillWidth: true
            Layout.topMargin: AppMetrics.gapCompact
            text: root.title
            font.weight: Font.DemiBold
            color: AppTheme.colourText
            elide: Text.ElideRight
            maximumLineCount: 1
        }

        FontMetrics
        {
            id: authorMetrics

            font.pixelSize: AppMetrics.fontSmall
            font.family: AppMetrics.fontFamily
        }

        DesktopLabel
        {
            Layout.fillWidth: true
            Layout.topMargin: AppMetrics.gapTight
            Layout.preferredHeight: authorMetrics.height
            text: root.author
            font.pixelSize: AppMetrics.fontSmall
            color: AppTheme.colourMuted
            elide: Text.ElideRight
            maximumLineCount: 1
        }

        RowLayout
        {
            Layout.fillWidth: true
            Layout.topMargin: AppMetrics.gapSmall
            spacing: AppMetrics.gapSmall

            ReaderLevelBadge
            {
                level: root.level
                textSize: AppMetrics.fontMicro
            }

            DesktopLabel
            {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                visible: root.transferring || (root.readPercent > 0 && !root.finished)
                elide: Text.ElideRight
                text: root.downloading ? qsTr("Downloading…")
                    : root.uploading ? qsTr("Uploading…")
                    : qsTr("%1%").arg(root.readPercent)
                font.pixelSize: AppMetrics.fontSmall
                color: AppTheme.colourMuted
            }

            DesktopIcon
            {
                visible: root.finished
                name: "check"
                color: AppTheme.colourCorrect
                width: AppMetrics.iconSizeSmall
                height: AppMetrics.iconSizeSmall

                Accessible.role: Accessible.StaticText
                Accessible.name: qsTr("Finished")
            }

            Item
            {
                Layout.fillWidth: true
            }

            DesktopIcon
            {
                visible: root.favorite
                name: "heart"
                filled: true
                color: AppTheme.colourWrong
                width: AppMetrics.iconSizeSmall
                height: AppMetrics.iconSizeSmall
            }

            DesktopIconButton
            {
                id: trailingButton

                objectName: "reader_book_menu"

                Layout.rightMargin: -AppMetrics.gapTight
                opacity: root.menuOffered ? 1 : 0
                enabled: root.menuOffered
                iconName: "kebab"
                iconColour: AppTheme.colourSubtle
                iconSize: AppMetrics.iconSizeSmall
                text: qsTr("Book details")

                onClicked: root.openOverview()
            }
        }
    }

    background: Item {}

    TapHandler
    {
        acceptedButtons: Qt.RightButton
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad

        onTapped: root.openOverview()
    }

    HoverHandler
    {
        cursorShape: root.transferring ? Qt.ArrowCursor : Qt.PointingHandCursor
    }
}
