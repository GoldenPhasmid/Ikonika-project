import QtQuick

Item
{
    id: root

    default property alias content: innerRing.content
    property real outerProgress: 0
    property real innerProgress: 0
    property color outerColour: AppTheme.colourAccent
    property color innerColour: AppTheme.colourCorrect
    property int sweepDuration: AppMetrics.motionSlow
    property real stroke: AppMetrics.goalRingStroke
    property real gap: AppMetrics.goalRingGap
    property bool showOuter: true
    property bool showInner: true

    implicitWidth: AppMetrics.goalRingSize
    implicitHeight: AppMetrics.goalRingSize

    Accessible.role: Accessible.ProgressBar
    Accessible.name: qsTr("Review %1 percent, learning %2 percent").arg(Math.round(outerRing.clamped * 100)).arg(Math.round(innerRing.clamped * 100))

    DesktopGoalRing
    {
        id: outerRing

        anchors.fill: parent

        progress: root.outerProgress
        fill: root.outerColour
        track: AppTheme.faded(root.outerColour, AppTheme.alphaSoft)
        sweepDuration: root.sweepDuration
        stroke: root.stroke
        showRing: root.showOuter
    }

    DesktopGoalRing
    {
        id: innerRing

        anchors.fill: parent
        anchors.margins: root.stroke + root.gap

        progress: root.innerProgress
        fill: root.innerColour
        track: AppTheme.faded(root.innerColour, AppTheme.alphaSoft)
        sweepDuration: root.sweepDuration
        stroke: root.stroke
        showRing: root.showInner
    }
}
