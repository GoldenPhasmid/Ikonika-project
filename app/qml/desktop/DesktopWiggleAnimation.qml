import QtQuick

SequentialAnimation
{
    id: root

    property Item target: null

    NumberAnimation { target: root.target; property: "rotation"; to: -AppMetrics.motionWiggle; duration: AppMetrics.motionSlow / 6 }
    NumberAnimation { target: root.target; property: "rotation"; to: AppMetrics.motionWiggle; duration: AppMetrics.motionSlow / 3 }
    NumberAnimation { target: root.target; property: "rotation"; to: -AppMetrics.motionWiggle / 2; duration: AppMetrics.motionSlow / 4 }
    NumberAnimation { target: root.target; property: "rotation"; to: 0; duration: AppMetrics.motionSlow / 4 }
}
