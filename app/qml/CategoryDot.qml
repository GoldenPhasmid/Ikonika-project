import QtQuick

Rectangle
{
    property color colour: AppTheme.colourMuted

    implicitWidth: AppMetrics.categoryDotSize
    implicitHeight: AppMetrics.categoryDotSize
    radius: AppMetrics.categoryDotSize / 3
    color: colour

    Accessible.ignored: true
}
