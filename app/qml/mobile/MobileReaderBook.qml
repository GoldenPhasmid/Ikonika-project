import QtQuick
import QtQuick.Layouts
import "MobileIcons.js" as Icons
import "../ReaderShelf.js" as Shelf

Item
{
    id: root

    property string title: ""
    property string format: ""
    property string level: ""
    property bool local: true
    property bool inCloud: false
    property bool free: false
    property real progress: -1
    property real size: 0
    property real readProgress: -1
    property bool compact: false

    signal clicked()
    signal actionsRequested()
    signal cancelRequested()

    readonly property bool transferring: root.progress >= 0
    readonly property bool downloading: root.transferring && !root.local
    readonly property bool catalogEntry: root.free && !root.local
    readonly property int percent: Math.round(Math.max(0, root.progress) * 100)
    readonly property bool showsBar: (root.transferring && root.local) || (!root.transferring && root.readProgress >= 0)
    readonly property real barValue: root.transferring ? root.progress : root.readProgress
    readonly property string brief: Shelf.brief(root)
    readonly property color coverColour: Shelf.tint(root.title, AppTheme.coverColours)
    readonly property string placeIcon: Icons.Icons[Shelf.place(root, "smartphone")].path
    readonly property string placeText: Shelf.placeText(root, qsTr("On this phone only"))

    implicitHeight: root.compact ? AppMetrics.readerRowHeight : Math.round(root.width / AppMetrics.readerTileAspect)

    Accessible.role: Accessible.Button
    Accessible.name: qsTr("%1, %2, %3 %4").arg(root.title).arg(root.format.toUpperCase()).arg(root.placeText).arg(root.brief)
        + (root.level.length > 0 ? ", " + Shelf.levelLabel(root.level) : "")

    Rectangle
    {
        id: cover

        anchors.fill: parent
        visible: !root.compact
        radius: AppMetrics.coverRadius
        color: root.coverColour
        opacity: root.local ? (coverTap.pressed ? AppTheme.opacityHover : 1.0) : AppTheme.opacityDisabled

        TapHandler
        {
            id: coverTap

            onTapped: root.clicked()
            onLongPressed: if (!root.transferring && !root.catalogEntry) root.actionsRequested()
        }

        CoverStripe {}

        RowLayout
        {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.leftMargin: AppMetrics.gapCompact
            anchors.topMargin: AppMetrics.gapCompact
            spacing: AppMetrics.gapSmall

            Text
            {
                text: root.format.toUpperCase()
                color: AppTheme.colourCoverInk
                opacity: AppTheme.opacitySecondary
                font.pixelSize: AppMetrics.fontMicro
                font.weight: Font.DemiBold
                font.family: AppMetrics.fontFamily
                font.letterSpacing: AppMetrics.borderWidth
            }

            MobileVectorIcon
            {
                Layout.preferredWidth: AppMetrics.fontBody
                Layout.preferredHeight: AppMetrics.fontBody

                svgPath: root.placeIcon
                color: AppTheme.colourCoverInk
                opacity: AppTheme.opacitySecondary
            }
        }

        Text
        {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: AppMetrics.gapCompact
            anchors.rightMargin: AppMetrics.gapCompact
            anchors.bottomMargin: AppMetrics.gapMedium + AppMetrics.coverProgressHeight

            text: root.title
            color: AppTheme.colourCoverInk
            font.pixelSize: AppMetrics.fontBody
            font.weight: Font.DemiBold
            font.family: AppMetrics.fontFamily
            wrapMode: Text.Wrap
            maximumLineCount: 2
            elide: Text.ElideRight
        }

        Rectangle
        {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: AppMetrics.gapCompact
            anchors.rightMargin: AppMetrics.gapCompact
            anchors.bottomMargin: AppMetrics.gapSmall
            height: AppMetrics.coverProgressHeight
            radius: height / 2
            visible: root.showsBar
            color: AppTheme.faded(AppTheme.colourCoverInk, AppTheme.alphaStrong)

            Rectangle
            {
                width: parent.width * Math.max(0, Math.min(1, root.barValue))
                height: parent.height
                radius: parent.radius
                color: AppTheme.colourCoverInk
            }
        }

        Item
        {
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: AppMetrics.gapTight
            width: AppMetrics.readerOpenIconSize + AppMetrics.gapSmall
            height: width
            visible: root.transferring && root.local

            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Cancel the upload")

            MobileVectorIcon
            {
                anchors.centerIn: parent
                width: AppMetrics.headerActionIconSize
                height: width

                svgPath: Icons.Icons.close.path
                color: AppTheme.colourCoverInk
            }

            TapHandler
            {
                gesturePolicy: TapHandler.ReleaseWithinBounds

                onTapped: root.cancelRequested()
            }
        }
    }

    ReaderLevelBadge
    {
        id: coverLevelBadge

        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: AppMetrics.gapCompact
        anchors.rightMargin: AppMetrics.gapCompact
        visible: !root.compact && !root.transferring && root.level.length > 0
        level: root.level
    }

    Rectangle
    {
        anchors.fill: parent
        visible: root.downloading && !root.compact
        radius: AppMetrics.coverRadius
        color: AppTheme.colourScrim

        Column
        {
            anchors.centerIn: parent
            spacing: AppMetrics.gapTight

            Text
            {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("%1%").arg(root.percent)
                color: AppTheme.colourCoverInk
                font.pixelSize: AppMetrics.fontLead
                font.weight: Font.DemiBold
                font.family: AppMetrics.fontFamily
            }

            Text
            {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("%1 of %2 MB").arg((root.size * Math.max(0, root.progress) / 1048576).toFixed(1))
                                         .arg((root.size / 1048576).toFixed(1))
                color: AppTheme.colourCoverInk
                opacity: AppTheme.opacityHover
                font.pixelSize: AppMetrics.fontMicro
                font.family: AppMetrics.fontFamily
            }

            Rectangle
            {
                anchors.horizontalCenter: parent.horizontalCenter
                width: cancelLabel.implicitWidth + AppMetrics.gapCompact * 2
                height: AppMetrics.readerOpenIconSize
                radius: height / 2
                color: AppTheme.colourText

                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Cancel the download")

                Text
                {
                    id: cancelLabel

                    anchors.centerIn: parent
                    text: qsTr("Cancel")
                    color: AppTheme.colourPanel
                    font.pixelSize: AppMetrics.fontMeta
                    font.weight: Font.Medium
                    font.family: AppMetrics.fontFamily
                }

                TapHandler
                {
                    margin: AppMetrics.gapSmall
                    gesturePolicy: TapHandler.ReleaseWithinBounds

                    onTapped: root.cancelRequested()
                }
            }
        }
    }

    Item
    {
        anchors.fill: parent
        visible: root.compact

        Rectangle
        {
            anchors.fill: parent
            color: AppTheme.colourFaint
            opacity: rowTap.pressed ? 1.0 : 0.0

            Behavior on opacity { NumberAnimation { duration: AppMetrics.motionFast } }
        }

        RowLayout
        {
            anchors.fill: parent
            spacing: 0

            Item
            {
                Layout.fillWidth: true
                Layout.fillHeight: true

                RowLayout
                {
                    anchors.fill: parent
                    spacing: AppMetrics.gapCompact

                    Rectangle
                    {
                        Layout.preferredWidth: AppMetrics.readerRowCoverWidth
                        Layout.preferredHeight: Math.round(AppMetrics.readerRowCoverWidth / AppMetrics.readerTileAspect)
                        radius: AppMetrics.radiusSmall
                        color: root.coverColour
                        opacity: root.local ? 1.0 : AppTheme.opacityDisabled

                        CoverStripe {}

                        Text
                        {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.margins: AppMetrics.gapTight + AppMetrics.borderWidth

                            text: root.format.toUpperCase()
                            color: AppTheme.colourCoverInk
                            opacity: AppTheme.opacityHover
                            font.pixelSize: AppMetrics.fontMicro - 2
                            font.weight: Font.DemiBold
                            font.family: AppMetrics.fontFamily
                            font.letterSpacing: AppMetrics.borderWidth
                        }

                        Rectangle
                        {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.margins: AppMetrics.gapTight + AppMetrics.borderWidth
                            height: AppMetrics.coverProgressHeight - AppMetrics.borderWidth
                            radius: height / 2
                            visible: root.showsBar
                            color: AppTheme.faded(AppTheme.colourCoverInk, AppTheme.alphaStrong)

                            Rectangle
                            {
                                width: parent.width * Math.max(0, Math.min(1, root.barValue))
                                height: parent.height
                                radius: parent.radius
                                color: AppTheme.colourCoverInk
                            }
                        }
                    }

                    ColumnLayout
                    {
                        Layout.fillWidth: true
                        spacing: AppMetrics.borderWidth * 3

                        Text
                        {
                            Layout.fillWidth: true

                            text: root.title
                            color: root.local ? AppTheme.colourText : AppTheme.colourMuted
                            font.pixelSize: AppMetrics.fontLabel
                            font.weight: Font.Medium
                            font.family: AppMetrics.fontFamily
                            elide: Text.ElideRight
                        }

                        RowLayout
                        {
                            Layout.fillWidth: true
                            spacing: AppMetrics.gapTight

                            MobileVectorIcon
                            {
                                Layout.preferredWidth: AppMetrics.fontLabel
                                Layout.preferredHeight: AppMetrics.fontLabel

                                svgPath: root.placeIcon
                                color: AppTheme.colourMuted
                            }

                            Text
                            {
                                Layout.fillWidth: true

                                text: root.brief
                                color: AppTheme.colourMuted
                                font.pixelSize: AppMetrics.fontSmall
                                font.family: AppMetrics.fontFamily
                                elide: Text.ElideRight
                            }

                            ReaderLevelBadge
                            {
                                level: root.level
                            }
                        }
                    }
                }

                TapHandler
                {
                    id: rowTap

                    onTapped: root.clicked()
                    onLongPressed: if (!root.transferring && !root.catalogEntry) root.actionsRequested()
                }
            }

            Item
            {
                Layout.preferredWidth: AppMetrics.buttonHeightLarge - AppMetrics.gapCompact
                Layout.fillHeight: true
                visible: !root.catalogEntry || root.downloading

                Accessible.role: Accessible.Button
                Accessible.name: root.transferring ? qsTr("Cancel") : qsTr("Book actions")

                MobileVectorIcon
                {
                    anchors.centerIn: parent
                    width: AppMetrics.headerActionIconSize
                    height: width

                    svgPath: root.transferring ? Icons.Icons.close.path : Icons.Icons.kebab.path
                    color: AppTheme.colourMuted
                }

                TapHandler
                {
                    onTapped: root.transferring ? root.cancelRequested() : root.actionsRequested()
                }
            }
        }

        Rectangle
        {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: AppMetrics.borderWidth
            color: AppTheme.colourBorder
        }
    }
}
