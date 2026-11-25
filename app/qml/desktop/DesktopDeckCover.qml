import QtQuick
import QtQuick.Layouts

Rectangle
{
    id: root

    property string title: ""
    property string iconName: ""
    property color tint: AppTheme.colourCoverMuted
    property int wordCount: 0
    property int learningCount: 0
    property int learnedCount: 0

    readonly property real wordCountSafe: Math.max(root.wordCount, 1)
    readonly property color inkColour: AppTheme.colourCoverInk

    radius: AppMetrics.coverRadius
    color: root.tint
    clip: true

    CoverStripe {}

    DesktopIcon
    {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: AppMetrics.gapMedium
        width: AppMetrics.deckIconSize
        height: AppMetrics.deckIconSize
        name: root.iconName
        color: root.inkColour
    }

    ColumnLayout
    {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: AppMetrics.gapMedium
        spacing: AppMetrics.gapSmall

        RowLayout
        {
            Layout.fillWidth: true
            spacing: AppMetrics.gapSmall

            DesktopLabel
            {
                Layout.fillWidth: true
                text: root.title
                font.pixelSize: AppMetrics.fontTitle
                font.weight: Font.DemiBold
                color: root.inkColour
                elide: Text.ElideRight
            }

            DesktopLabel
            {
                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                text: root.wordCount
                font.pixelSize: AppMetrics.fontSmall
                color: root.inkColour
                opacity: AppTheme.opacitySecondary
            }
        }

        Rectangle
        {
            Layout.fillWidth: true
            implicitHeight: AppMetrics.coverProgressHeight
            radius: height / 2
            color: AppTheme.faded(root.inkColour, AppTheme.alphaStrong)
            clip: true

            Row
            {
                anchors.fill: parent

                Rectangle
                {
                    width: parent.width * root.learnedCount / root.wordCountSafe
                    height: parent.height
                    color: root.inkColour
                }

                Rectangle
                {
                    width: parent.width * root.learningCount / root.wordCountSafe
                    height: parent.height
                    color: AppTheme.faded(root.inkColour, AppTheme.alphaHalf)
                }
            }
        }
    }
}
