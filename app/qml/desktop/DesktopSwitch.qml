import QtQuick

Rectangle
{
    id: root

    property bool on: false

    implicitWidth: AppMetrics.chipHeight + AppMetrics.gapCompact
    implicitHeight: AppMetrics.badgeHeight

    radius: height / 2
    color: root.on ? AppTheme.colourAccent : AppTheme.colourFaint
    border.width: root.on ? 0 : AppMetrics.borderWidth
    border.color: AppTheme.colourBorder

    Behavior on color { ColorAnimation { duration: AppMetrics.motionFast } }

    Rectangle
    {
        width: parent.height - AppMetrics.gapTight * 2
        height: width
        radius: width / 2
        anchors.verticalCenter: parent.verticalCenter
        x: root.on ? parent.width - width - AppMetrics.gapTight : AppMetrics.gapTight
        color: root.on ? AppTheme.colourAccentText : AppTheme.colourMuted

        Behavior on x { NumberAnimation { duration: AppMetrics.motionFast; easing.type: AppMetrics.motionEasing } }
    }
}
