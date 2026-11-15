import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Button
{
    id: root

    property string author: ""
    property string coverSource: ""
    property real progress: 0

    readonly property int percent: Math.ceil(Math.max(0, root.progress) * 100)

    padding: AppMetrics.gapCompact

    hoverEnabled: true
    focusPolicy: Qt.StrongFocus

    z: root.hovered ? 1 : 0
    scale: root.hovered ? AppMetrics.coverHoverScale : 1.0

    Behavior on scale { NumberAnimation { duration: AppMetrics.motionBase; easing.type: Easing.OutCubic } }

    Accessible.name: qsTr("Continue %1, %2% read").arg(root.text).arg(root.percent)
    Accessible.description: root.author

    contentItem: RowLayout
    {
        spacing: AppMetrics.gapMedium

        DesktopBookCover
        {
            Layout.fillHeight: true
            Layout.preferredWidth: Math.round(height / AppMetrics.bookCoverAspect)
            title: root.text
            author: root.author
            imageSource: root.coverSource
        }

        ColumnLayout
        {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: AppMetrics.gapTight

            DesktopLabel
            {
                Layout.fillWidth: true
                text: root.text
                font.pixelSize: AppMetrics.fontLabel
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            DesktopLabel
            {
                Layout.fillWidth: true
                visible: text.length > 0
                text: root.author
                font.pixelSize: AppMetrics.fontSmall
                color: AppTheme.colourMuted
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            Item
            {
                Layout.fillHeight: true
            }

            RowLayout
            {
                Layout.fillWidth: true
                spacing: AppMetrics.gapSmall

                DesktopProgressBar
                {
                    Layout.fillWidth: true
                    progress: root.progress
                    fill: AppTheme.colourAccent
                }

                DesktopLabel
                {
                    text: qsTr("%1%").arg(root.percent)
                    font.pixelSize: AppMetrics.fontSmall
                    font.features: { "tnum": 1 }
                    color: AppTheme.colourMuted
                }

                DesktopIcon
                {
                    name: "arrow_right"
                    color: root.hovered ? AppTheme.colourText : AppTheme.colourSubtle
                    width: AppMetrics.iconSizeSmall
                    height: AppMetrics.iconSizeSmall
                }
            }
        }
    }

    background: Rectangle
    {
        radius: AppMetrics.radiusLarge
        color: root.down ? AppTheme.colourFaint : (root.hovered ? AppTheme.colourSunken : AppTheme.colourPanel)
        border.width: AppMetrics.borderWidth
        border.color: root.hovered ? AppTheme.colourOutline : AppTheme.colourBorder

        Behavior on color { ColorAnimation { duration: AppMetrics.motionFast } }

        DesktopFocusRing
        {
            visible: root.visualFocus
            baseRadius: AppMetrics.radiusLarge
        }
    }

    HoverHandler
    {
        cursorShape: Qt.PointingHandCursor
    }
}
