import QtQuick
import QtQuick.Controls.Basic

ScrollBar
{
    id: root

    padding: AppMetrics.borderWidth * 2
    minimumSize: 0.08
    policy: ScrollBar.AsNeeded
    visible: root.size < 1.0

    contentItem: Rectangle
    {
        implicitWidth: AppMetrics.scrollBarWidth
        implicitHeight: AppMetrics.scrollBarWidth
        radius: width / 2

        color: root.pressed ? AppTheme.colourOutline : AppTheme.colourBorder
        opacity: root.active || root.hovered ? 1.0 : 0.0

        Behavior on opacity { NumberAnimation { duration: AppMetrics.motionBase } }
    }

    background: Item {}
}
