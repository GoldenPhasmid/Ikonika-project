import QtQuick
import QtQuick.Layouts
import "MobileIcons.js" as Icons
import "../Decks.js" as Decks

Item
{
    id: root

    property string title: ""
    property string iconName: ""
    property color tint: AppTheme.colourCoverMuted
    property int wordCount: 0
    property int learningCount: 0
    property int learnedCount: 0
    property int untriagedCount: 0
    property bool compact: false

    signal trainRequested()
    signal openRequested()

    readonly property bool trainable: root.untriagedCount > 0
    readonly property real wordCountSafe: Math.max(root.wordCount, 1)
    readonly property string iconPath: Icons.Icons[root.iconName] ? Icons.Icons[root.iconName].path : ""
    readonly property string detail: Decks.detail(root)

    function activate()
    {
        if (root.trainable)
        {
            root.trainRequested()

            return
        }

        root.openRequested()
    }

    implicitHeight: root.compact ? AppMetrics.readerRowHeight : Math.round(root.width / AppMetrics.readerTileAspect)

    Accessible.role: Accessible.Button
    Accessible.name: root.trainable ? qsTr("Train %1, %2").arg(root.title).arg(root.detail)
                                    : qsTr("Open %1, %2").arg(root.title).arg(root.detail)

    component Progress: Rectangle
    {
        radius: height / 2
        color: AppTheme.faded(AppTheme.colourCoverInk, AppTheme.alphaStrong)
        clip: true

        Row
        {
            anchors.fill: parent

            Rectangle
            {
                width: parent.width * root.learnedCount / root.wordCountSafe
                height: parent.height
                color: AppTheme.colourCoverInk
            }

            Rectangle
            {
                width: parent.width * root.learningCount / root.wordCountSafe
                height: parent.height
                color: AppTheme.faded(AppTheme.colourCoverInk, AppTheme.alphaHalf)
            }
        }
    }

    component WordsButton: Item
    {
        property color iconColour: AppTheme.colourCoverInk

        Accessible.role: Accessible.Button
        Accessible.name: qsTr("Words of %1").arg(root.title)

        MobileVectorIcon
        {
            anchors.centerIn: parent
            width: AppMetrics.headerActionIconSize
            height: width

            svgPath: Icons.Icons.contents.path
            color: parent.iconColour
        }

        TapHandler
        {
            gesturePolicy: TapHandler.ReleaseWithinBounds

            onTapped: root.openRequested()
        }
    }

    Rectangle
    {
        anchors.fill: parent
        visible: !root.compact
        radius: AppMetrics.coverRadius
        color: root.tint
        opacity: coverTap.pressed ? AppTheme.opacityHover : 1.0

        TapHandler
        {
            id: coverTap

            onTapped: root.activate()
        }

        CoverStripe {}

        MobileVectorIcon
        {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: AppMetrics.gapCompact
            width: AppMetrics.deckIconSize
            height: width

            svgPath: root.iconPath
            color: AppTheme.colourCoverInk
        }

        WordsButton
        {
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: AppMetrics.gapTight
            width: AppMetrics.readerOpenIconSize + AppMetrics.gapSmall
            height: width
        }

        ColumnLayout
        {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: AppMetrics.gapCompact
            anchors.bottomMargin: AppMetrics.gapSmall
            spacing: AppMetrics.gapSmall

            RowLayout
            {
                Layout.fillWidth: true
                spacing: AppMetrics.gapTight

                Text
                {
                    Layout.fillWidth: true

                    text: root.title
                    color: AppTheme.colourCoverInk
                    font.pixelSize: AppMetrics.fontBody
                    font.weight: Font.DemiBold
                    font.family: AppMetrics.fontFamily
                    elide: Text.ElideRight
                }

                Text
                {
                    text: root.wordCount
                    color: AppTheme.colourCoverInk
                    opacity: AppTheme.opacitySecondary
                    font.pixelSize: AppMetrics.fontMeta
                    font.family: AppMetrics.fontFamily
                }
            }

            Progress
            {
                Layout.fillWidth: true
                Layout.preferredHeight: AppMetrics.coverProgressHeight
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
                        color: root.tint

                        CoverStripe {}

                        MobileVectorIcon
                        {
                            anchors.centerIn: parent
                            width: AppMetrics.headerActionIconSize
                            height: width

                            svgPath: root.iconPath
                            color: AppTheme.colourCoverInk
                        }

                        Progress
                        {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.margins: AppMetrics.gapTight + AppMetrics.borderWidth
                            height: AppMetrics.coverProgressHeight - AppMetrics.borderWidth
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
                            color: AppTheme.colourText
                            font.pixelSize: AppMetrics.fontLabel
                            font.weight: Font.Medium
                            font.family: AppMetrics.fontFamily
                            elide: Text.ElideRight
                        }

                        Text
                        {
                            Layout.fillWidth: true

                            text: root.detail
                            color: AppTheme.colourMuted
                            font.pixelSize: AppMetrics.fontSmall
                            font.family: AppMetrics.fontFamily
                            elide: Text.ElideRight
                        }
                    }
                }

                TapHandler
                {
                    id: rowTap

                    onTapped: root.activate()
                }
            }

            WordsButton
            {
                Layout.preferredWidth: AppMetrics.buttonHeightLarge - AppMetrics.gapCompact
                Layout.fillHeight: true
                iconColour: AppTheme.colourMuted
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
