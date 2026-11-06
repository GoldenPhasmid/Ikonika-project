import QtQuick

Item
{
    id: root

    property real outerProgress: 0
    property real innerProgress: 0
    property color outerColour: AppTheme.colourAccent
    property color innerColour: AppTheme.colourCorrect
    property real stroke: AppMetrics.goalRingStroke
    property real gap: stroke / 4
    property bool showOuter: true
    property bool showInner: true

    implicitWidth: AppMetrics.goalRingSize
    implicitHeight: AppMetrics.goalRingSize

    MobileGoalRing
    {
        anchors.fill: parent
        visible: root.showOuter

        progress: root.outerProgress
        fill: root.outerColour
        track: AppTheme.faded(root.outerColour, AppTheme.alphaSoft)
        stroke: root.stroke
    }

    MobileGoalRing
    {
        anchors.fill: parent
        anchors.margins: root.stroke + root.gap
        visible: root.showInner

        progress: root.innerProgress
        fill: root.innerColour
        track: AppTheme.faded(root.innerColour, AppTheme.alphaSoft)
        stroke: root.stroke
    }
}
