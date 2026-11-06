import QtQuick
import QtQuick.Controls.Basic

Menu
{
    id: root

    padding: AppMetrics.gapTight

    delegate: DesktopMenuItem {}

    background: Rectangle
    {
        implicitWidth: AppMetrics.menuWidth
        radius: AppMetrics.radiusMedium
        color: AppTheme.colourPanel
        border.width: AppMetrics.borderWidth
        border.color: AppTheme.colourBorder
    }

    enter: Transition
    {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: AppMetrics.motionFast }
    }

    exit: Transition
    {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: AppMetrics.motionFast }
    }
}
