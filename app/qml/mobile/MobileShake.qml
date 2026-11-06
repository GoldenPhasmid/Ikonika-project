import QtQuick

Translate
{
    id: root

    property real reach: AppMetrics.motionShake

    function play()
    {
        animation.restart()
    }

    property SequentialAnimation animation: SequentialAnimation
    {
        NumberAnimation { target: root; property: "x"; to: -root.reach; duration: AppMetrics.motionSlow / 6 }
        NumberAnimation { target: root; property: "x"; to: root.reach; duration: AppMetrics.motionSlow / 3 }
        NumberAnimation { target: root; property: "x"; to: -root.reach / 2; duration: AppMetrics.motionSlow / 4 }
        NumberAnimation { target: root; property: "x"; to: 0; duration: AppMetrics.motionSlow / 4 }
    }
}
