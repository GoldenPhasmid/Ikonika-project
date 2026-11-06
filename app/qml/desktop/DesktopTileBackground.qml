import QtQuick

Rectangle
{
    id: root

    property bool hovered: false
    property bool focused: false

    radius: AppMetrics.radiusLarge
    color: root.hovered ? AppTheme.colourSunken : AppTheme.colourPanel
    border.width: AppMetrics.borderWidth
    border.color: AppTheme.colourBorder

    Behavior on color { ColorAnimation { duration: AppMetrics.motionFast } }

    DesktopFocusRing
    {
        visible: root.focused
        baseRadius: AppMetrics.radiusLarge
    }
}
