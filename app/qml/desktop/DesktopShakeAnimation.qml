import QtQuick

SequentialAnimation
{
    id: root

    property QtObject target: null

    NumberAnimation { target: root.target; property: "shakeOffset"; to: -AppMetrics.motionShake; duration: AppMetrics.motionSlow / 6 }
    NumberAnimation { target: root.target; property: "shakeOffset"; to: AppMetrics.motionShake; duration: AppMetrics.motionSlow / 3 }
    NumberAnimation { target: root.target; property: "shakeOffset"; to: -AppMetrics.motionShake / 2; duration: AppMetrics.motionSlow / 4 }
    NumberAnimation { target: root.target; property: "shakeOffset"; to: 0; duration: AppMetrics.motionSlow / 4 }
}
