import QtQuick

Rectangle
{
    property real baseRadius: AppMetrics.radiusMedium

    anchors.fill: parent
    anchors.margins: -AppMetrics.focusRingWidth * 2

    radius: baseRadius + AppMetrics.focusRingWidth * 2
    color: "transparent"
    border.width: AppMetrics.focusRingWidth
    border.color: AppTheme.colourAccent
}
