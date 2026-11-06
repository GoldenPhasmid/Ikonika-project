import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

MenuItem
{
    id: root

    property bool destructive: false
    property string iconName: ""
    property int stage: -1
    property bool completed: false

    implicitHeight: AppMetrics.buttonHeight
    leftPadding: AppMetrics.gapCompact
    rightPadding: AppMetrics.gapCompact
    opacity: root.enabled ? 1.0 : AppTheme.opacityDisabled

    contentItem: RowLayout
    {
        spacing: AppMetrics.gapSmall

        DesktopIcon
        {
            visible: root.iconName.length > 0
            name: root.iconName
            width: AppMetrics.iconSizeSmall
            height: AppMetrics.iconSizeSmall
            color: root.destructive ? AppTheme.colourWrong : AppTheme.colourText
        }

        DesktopLabel
        {
            Layout.fillWidth: true
            text: root.text
            color: root.destructive ? AppTheme.colourWrong : AppTheme.colourText
            elide: Text.ElideRight
        }

        DesktopLearnedMark
        {
            stage: root.stage
            completed: root.completed
        }
    }

    background: Rectangle
    {
        radius: AppMetrics.radiusSmall
        color: root.highlighted || root.hovered ? AppTheme.colourHover : AppTheme.faded(AppTheme.colourHover, 0)
    }
}
