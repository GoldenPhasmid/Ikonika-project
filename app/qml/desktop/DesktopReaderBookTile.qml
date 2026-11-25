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
    property bool catalogEntry: false
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
    readonly property int percent: Math.round(Math.max(0, root.progress) * 100)
    readonly property int readPercent: Math.ceil(Math.max(0, root.readProgress) * 100)
    readonly property real actionOverhang: (AppMetrics.iconTapSize - AppMetrics.iconSize) / 2

    signal removeRequested()
    signal favoriteToggled()
    signal overviewRequested()
    signal cancelRequested()

    component RowAction: DesktopIconButton
    {
        Layout.rightMargin: -root.actionOverhang
        focusPolicy: Qt.NoFocus
        iconColour: AppTheme.colourSubtle
    }

    function openOverview()
    {
        if (root.menuOffered)
        {
            root.overviewRequested()
        }
    }

    onClicked:
    {
        if (root.transferring)
        {
            root.cancelRequested()
        }
    }

    onPressAndHold: root.openOverview()

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
            Layout.topMargin: AppMetrics.gapTight
            Layout.preferredHeight: AppMetrics.iconTapSize
            spacing: 0

            ReaderLevelBadge
            {
                id: levelBadge

                Layout.preferredWidth: AppMetrics.bookLevelSize
                Layout.preferredHeight: AppMetrics.bookLevelSize
                solid: true
                level: root.level
                textSize: AppMetrics.bookLevelFont
            }

            RowAction
            {
                objectName: "reader_book_favorite"

                Layout.leftMargin: levelBadge.visible ? (AppMetrics.iconTapSize - AppMetrics.bookLevelSize) / 2 : -root.actionOverhang
                Layout.rightMargin: 0
                visible: !root.catalogEntry
                iconName: "heart"
                iconFilled: root.favorite
                iconColour: root.favorite ? AppTheme.colourWrong : AppTheme.colourSubtle
                text: root.favorite ? qsTr("Remove from favorites") : qsTr("Add to favorites")

                onClicked: root.favoriteToggled()
            }

            Item
            {
                Layout.fillWidth: true
            }

            RowAction
            {
                objectName: "reader_book_download"

                visible: root.downloadable
                iconName: "download"
                text: qsTr("Download")

                onClicked: root.clicked()
            }

            RowAction
            {
                id: transferAction

                objectName: "reader_book_cancel"

                visible: root.transferring
                iconName: transferAction.hovered ? "close" : ""
                text: root.uploading ? qsTr("Cancel the upload") : qsTr("Cancel the download")

                onClicked: root.cancelRequested()

                DesktopGoalRing
                {
                    anchors.centerIn: parent
                    width: AppMetrics.iconSize
                    height: AppMetrics.iconSize
                    visible: !transferAction.hovered
                    progress: Math.max(0, root.progress)
                    stroke: AppMetrics.focusRingWidth
                    fill: AppTheme.colourSubtle
                    track: AppTheme.faded(AppTheme.colourSubtle, AppTheme.alphaStrong)
                    sweepDuration: AppMetrics.motionFast
                }
            }

            RowAction
            {
                objectName: "reader_book_menu"

                visible: root.menuOffered
                iconName: "kebab"
                iconFilled: true
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
        cursorShape: Qt.PointingHandCursor
    }
}
