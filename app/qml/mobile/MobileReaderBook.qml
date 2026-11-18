import QtQuick
import QtQuick.Layouts
import "../ReaderShelf.js" as Shelf

Item
{
    id: root

    required property var book
    property real progress: -1
    property bool cloudAvailable: false
    property bool compact: false

    signal clicked()
    signal detailsRequested()
    signal cancelRequested()
    signal downloadRequested()
    signal favoriteToggled()
    signal collectionsRequested()

    readonly property bool local: root.book.local
    readonly property bool free: root.book.free === true || root.book.freeId !== undefined
    readonly property bool catalogEntry: !root.local && root.book.freeId !== undefined
    readonly property bool transferring: root.progress >= 0
    readonly property bool downloading: root.transferring && !root.local
    readonly property bool uploading: root.transferring && root.local
    readonly property bool onlyInCloud: !root.local && !root.transferring
    readonly property bool downloadable: root.onlyInCloud && (root.free || root.cloudAvailable)
    readonly property bool finished: root.book.finished === true
    readonly property bool favorite: root.book.favorite === true
    readonly property real readProgress: Math.max(0, root.book.progress)
    readonly property int readPercent: Math.ceil(root.readProgress * 100)
    readonly property bool detailsOffered: root.local && root.book.exists && !root.transferring
    readonly property int percent: Math.round(Math.max(0, root.progress) * 100)
    readonly property real coverWidth: root.compact ? Math.round(AppMetrics.readerRowCoverWidth * 1.5) : root.width
    readonly property real coverHeight: Math.round(root.coverWidth * AppMetrics.bookCoverAspect)
    readonly property real rowIconSize: AppMetrics.headerActionIconSize + AppMetrics.gapTight
    readonly property string transferText: root.downloading ? qsTr("Downloading…") : qsTr("Uploading…")

    implicitHeight: root.compact ? Math.max(root.coverHeight, listColumn.implicitHeight) + AppMetrics.gapMedium * 2
                                 : tile.implicitHeight

    Accessible.role: Accessible.Button
    Accessible.name: [root.book.title, root.book.author || "",
                      root.transferring ? root.transferText : root.onlyInCloud ? qsTr("Not downloaded") : "",
                      root.finished ? qsTr("Finished") : root.readPercent > 0 ? qsTr("%1% read").arg(root.readPercent) : "",
                      root.favorite ? qsTr("Favorite") : "", Shelf.levelLabel(root.book.level || "")]
                         .filter(function(part) { return part.length > 0 }).join(", ")
    Accessible.onPressAction: root.clicked()

    component Cover: Item
    {
        id: coverItem

        property real ringSize: AppMetrics.bookRingSize

        DesktopBookCover
        {
            anchors.fill: parent
            opacity: coverTap.pressed ? AppTheme.opacityHover : 1.0

            title: root.book.title
            author: root.book.author || ""
            imageSource: root.book.cover || ""
            progressShown: !root.compact && !root.transferring && root.readProgress > 0 && !root.finished
            progress: root.readProgress
        }

        Rectangle
        {
            objectName: "reader_book_download"

            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: AppMetrics.gapSmall
            width: AppMetrics.buttonHeightLarge
            height: width
            radius: width / 2
            visible: !root.compact && root.downloadable
            color: AppTheme.faded(AppTheme.colourScrim, AppTheme.opacitySecondary)

            DesktopIcon
            {
                anchors.centerIn: parent
                name: "download"
                color: AppTheme.colourCoverInk
                width: AppMetrics.headerActionIconSize
                height: AppMetrics.headerActionIconSize
            }
        }

        Loader
        {
            anchors.fill: parent
            active: root.transferring

            sourceComponent: Component
            {
                Rectangle
                {
                    radius: AppMetrics.bookRadius
                    color: AppTheme.faded(AppTheme.colourScrim, AppTheme.opacityHover)

                    DesktopGoalRing
                    {
                        anchors.centerIn: parent
                        width: coverItem.ringSize
                        height: coverItem.ringSize
                        progress: Math.max(0, root.progress)
                        stroke: AppMetrics.bookRingStroke
                        fill: AppTheme.colourCoverInk
                        track: AppTheme.faded(AppTheme.colourCoverInk, AppTheme.alphaStrong)
                        sweepDuration: AppMetrics.motionFast

                        Text
                        {
                            text: qsTr("%1%").arg(root.percent)
                            font.pixelSize: AppMetrics.fontSmall
                            font.weight: Font.DemiBold
                            font.family: AppMetrics.fontFamily
                            color: AppTheme.colourCoverInk
                        }
                    }
                }
            }
        }

        TapHandler
        {
            id: coverTap

            onTapped: root.clicked()
            onLongPressed: if (root.detailsOffered) root.detailsRequested()
        }
    }

    component Favorite: MobileIconButton
    {
        objectName: "reader_book_favorite"

        iconName: "heart"
        filled: root.favorite
        iconColour: root.favorite ? AppTheme.colourWrong : AppTheme.colourSubtle
        text: root.favorite ? qsTr("Remove from favorites") : qsTr("Add to favorites")

        onClicked: root.favoriteToggled()
    }

    ColumnLayout
    {
        id: tile

        width: parent.width
        visible: !root.compact
        spacing: 0

        Cover
        {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.round(width * AppMetrics.bookCoverAspect)
        }

        Text
        {
            Layout.fillWidth: true
            Layout.topMargin: AppMetrics.gapCompact
            text: root.book.title
            font.pixelSize: AppMetrics.fontTitle
            font.weight: Font.DemiBold
            font.family: AppMetrics.fontFamily
            color: AppTheme.colourText
            elide: Text.ElideRight
            maximumLineCount: 1
        }

        Text
        {
            Layout.fillWidth: true
            Layout.topMargin: AppMetrics.gapTight
            text: root.book.author || " "
            font.pixelSize: AppMetrics.fontBody
            font.family: AppMetrics.fontFamily
            color: AppTheme.colourMuted
            elide: Text.ElideRight
            maximumLineCount: 1
        }

        RowLayout
        {
            Layout.fillWidth: true
            Layout.preferredHeight: AppMetrics.exerciseActionIconTapWidth
            spacing: AppMetrics.gapSmall

            ReaderLevelBadge
            {
                level: root.book.level || ""
                textSize: AppMetrics.fontSmall
            }

            Text
            {
                Layout.minimumWidth: 0
                visible: root.transferring || (root.readPercent > 0 && !root.finished)
                text: root.transferring ? root.transferText : qsTr("%1%").arg(root.readPercent)
                font.pixelSize: AppMetrics.fontBody
                font.family: AppMetrics.fontFamily
                font.features: { "tnum": 1 }
                color: AppTheme.colourMuted
                elide: Text.ElideRight
            }

            DesktopIcon
            {
                visible: root.finished
                name: "check"
                color: AppTheme.colourCorrect
                width: AppMetrics.headerActionIconSize
                height: AppMetrics.headerActionIconSize
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
                width: AppMetrics.headerActionIconSize
                height: AppMetrics.headerActionIconSize
            }

            MobileIconButton
            {
                objectName: "reader_book_details"

                Layout.preferredWidth: AppMetrics.exerciseActionIconTapWidth
                Layout.preferredHeight: AppMetrics.exerciseActionIconTapWidth
                Layout.rightMargin: -(AppMetrics.exerciseActionIconTapWidth - AppMetrics.headerActionIconSize) / 2
                visible: root.detailsOffered
                iconName: "kebab"
                iconSize: AppMetrics.headerActionIconSize
                iconColour: AppTheme.colourSubtle
                text: qsTr("Book details")

                onClicked: root.detailsRequested()
            }
        }
    }

    Item
    {
        anchors.fill: parent
        anchors.leftMargin: AppMetrics.contentPadding
        anchors.rightMargin: AppMetrics.contentPadding
        anchors.topMargin: AppMetrics.gapMedium
        anchors.bottomMargin: AppMetrics.gapMedium
        visible: root.compact

        Cover
        {
            id: listCover

            width: root.coverWidth
            height: root.coverHeight
            ringSize: AppMetrics.buttonHeightLarge
        }

        ColumnLayout
        {
            id: listColumn

            anchors.left: listCover.right
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: AppMetrics.contentPadding
            height: Math.max(listCover.height, implicitHeight)
            spacing: 0

            Text
            {
                Layout.fillWidth: true
                text: root.book.title
                font.pixelSize: AppMetrics.fontSubtitle
                font.weight: Font.DemiBold
                font.family: AppMetrics.fontFamily
                color: AppTheme.colourText
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }

            Text
            {
                Layout.fillWidth: true
                Layout.topMargin: AppMetrics.gapTight
                visible: text.length > 0
                text: root.book.author || ""
                font.pixelSize: AppMetrics.fontLabel
                font.family: AppMetrics.fontFamily
                color: AppTheme.colourMuted
                elide: Text.ElideRight
            }

            Item
            {
                Layout.fillHeight: true
            }

            RowLayout
            {
                Layout.fillWidth: true
                Layout.preferredHeight: root.rowIconSize
                spacing: AppMetrics.gapCompact

                Text
                {
                    Layout.fillWidth: true
                    visible: root.transferring || (root.onlyInCloud && !root.catalogEntry)
                    text: root.transferring ? root.transferText : qsTr("In the cloud")
                    font.pixelSize: AppMetrics.fontBody
                    font.family: AppMetrics.fontFamily
                    color: AppTheme.colourMuted
                    elide: Text.ElideRight
                }

                Rectangle
                {
                    Layout.fillWidth: true
                    Layout.preferredHeight: AppMetrics.coverProgressHeight
                    visible: !root.transferring && !root.onlyInCloud
                    radius: height / 2
                    color: AppTheme.colourBorder

                    Rectangle
                    {
                        width: parent.width * (root.finished ? 1 : root.readProgress)
                        height: parent.height
                        radius: parent.radius
                        color: root.finished ? AppTheme.colourCorrect : AppTheme.colourAccent
                    }
                }

                Item
                {
                    Layout.preferredWidth: AppMetrics.exerciseActionIconTapWidth
                    Layout.fillHeight: true
                    visible: !root.transferring && !root.onlyInCloud

                    Text
                    {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        visible: !root.finished
                        text: qsTr("%1%").arg(root.readPercent)
                        font.pixelSize: AppMetrics.fontBody
                        font.family: AppMetrics.fontFamily
                        font.features: { "tnum": 1 }
                        color: AppTheme.colourMuted
                    }

                    DesktopIcon
                    {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        visible: root.finished
                        name: "check"
                        color: AppTheme.colourCorrect
                        width: AppMetrics.headerActionIconSize
                        height: AppMetrics.headerActionIconSize
                    }
                }
            }

            RowLayout
            {
                Layout.fillWidth: true
                Layout.topMargin: AppMetrics.gapTight - (AppMetrics.buttonHeightLarge - root.rowIconSize) / 2
                Layout.rightMargin: -(AppMetrics.exerciseActionIconTapWidth - root.rowIconSize) / 2
                Layout.bottomMargin: -(AppMetrics.buttonHeightLarge - root.rowIconSize) / 2
                spacing: 0

                ReaderLevelBadge
                {
                    level: root.book.level || ""
                    textSize: AppMetrics.fontSmall
                }

                Item
                {
                    Layout.fillWidth: true
                }

                MobileIconButton
                {
                    objectName: "reader_book_download_action"

                    visible: root.downloadable
                    iconName: "download"
                    iconColour: AppTheme.colourAccent
                    iconSize: root.rowIconSize
                    text: qsTr("Download")

                    onClicked: root.downloadRequested()
                }

                MobileIconButton
                {
                    objectName: "reader_book_cancel"

                    visible: root.transferring
                    iconName: "close"
                    iconSize: root.rowIconSize
                    iconColour: AppTheme.colourSubtle
                    text: root.uploading ? qsTr("Cancel the upload") : qsTr("Cancel the download")

                    onClicked: root.cancelRequested()
                }

                Favorite
                {
                    visible: !root.catalogEntry
                    iconSize: root.rowIconSize
                }

                MobileIconButton
                {
                    objectName: "reader_book_collections"

                    visible: !root.catalogEntry
                    iconName: "folder_plus"
                    iconSize: root.rowIconSize
                    iconColour: AppTheme.colourSubtle
                    text: qsTr("Collections")

                    onClicked: root.collectionsRequested()
                }

                MobileIconButton
                {
                    objectName: "reader_book_details"

                    visible: root.detailsOffered
                    iconName: "info"
                    iconSize: root.rowIconSize
                    iconColour: AppTheme.colourSubtle
                    text: qsTr("Book details")

                    onClicked: root.detailsRequested()
                }
            }
        }
    }

    Rectangle
    {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: AppMetrics.borderWidth
        visible: root.compact
        color: AppTheme.colourBorder
    }
}
