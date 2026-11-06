import QtQuick
import QtQuick.Layouts

Item
{
    id: root

    property int stage: 0
    property int stageCount: 0
    property real stageProgress: 0

    implicitWidth: root.stageCount * AppMetrics.stageSegmentWidth + Math.max(0, root.stageCount - 1) * AppMetrics.gapSmall
    implicitHeight: AppMetrics.progressBarHeight

    RowLayout
    {
        id: segments

        objectName: "progress_segments"

        anchors.centerIn: parent
        width: Math.min(root.implicitWidth, root.width)
        spacing: AppMetrics.gapSmall
        visible: root.stageCount > 0

        Accessible.role: Accessible.ProgressBar
        Accessible.name: qsTr("Stage %1 of %2").arg(Math.min(root.stage, root.stageCount)).arg(root.stageCount)

        Repeater
        {
            model: root.stageCount

            delegate: Rectangle
            {
                required property int index

                readonly property bool current: index + 1 === root.stage
                readonly property bool done: index + 1 < root.stage

                Layout.fillWidth: true
                Layout.preferredWidth: 1
                implicitHeight: AppMetrics.progressBarHeight
                radius: height / 2
                color: current ? AppTheme.colourText : (done ? AppTheme.colourAccent : AppTheme.colourBorder)

                Rectangle
                {
                    objectName: "stage_fill"

                    visible: parent.current
                    width: parent.width * (parent.current ? Math.max(0, Math.min(1, root.stageProgress)) : 0)
                    height: parent.height
                    radius: parent.radius
                    color: AppTheme.colourAccent

                    Behavior on width
                    {
                        NumberAnimation { duration: AppMetrics.motionSlow; easing.type: AppMetrics.motionEasing }
                    }
                }
            }
        }
    }
}
