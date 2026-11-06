import QtQuick
import QtQuick.Controls.Basic
import "ReaderShelf.js" as Shelf

Rectangle
{
    id: root

    required property string level
    property real textSize: AppMetrics.fontMicro

    readonly property string label: Shelf.levelLabel(root.level)
    readonly property string mark: ["", "A", "B", "C"][Shelf.levels.indexOf(root.level)] || ""
    readonly property color baseColour: root.level === "beginner" ? AppTheme.colourCorrect
        : root.level === "intermediate" ? AppTheme.colourAccent : AppTheme.colourWarn
    readonly property color levelColour: AppTheme.colourPanel.hslLightness > 0.5 ? Qt.darker(root.baseColour, 1.2)
                                                                            : root.baseColour

    implicitWidth: badgeLabel.implicitWidth + AppMetrics.gapSmall * 2
    implicitHeight: badgeLabel.implicitHeight + AppMetrics.gapTight * 2
    visible: root.label.length > 0
    radius: AppMetrics.radiusSmall
    color: AppTheme.colourPanel
    border.width: AppMetrics.borderWidth
    border.color: root.levelColour

    ToolTip.visible: hover.hovered && root.label.length > 0
    ToolTip.text: root.label
    ToolTip.delay: AppMetrics.tooltipDelay

    HoverHandler
    {
        id: hover
    }

    Text
    {
        id: badgeLabel

        anchors.centerIn: parent
        text: root.mark
        Accessible.name: root.label
        color: root.levelColour
        font.pixelSize: root.textSize
        font.weight: Font.DemiBold
        font.family: AppMetrics.fontFamily
    }
}
