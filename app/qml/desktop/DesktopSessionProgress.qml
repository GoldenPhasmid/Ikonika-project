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

        x: Math.round((root.width - segments.width) / 2)
        y:
        {
            const centred = (root.height - segments.height) / 2
            const sceneY = root.parent ? root.parent.mapToItem(null, 0, root.y + centred).y : root.y + centred
            const ratio = Screen.devicePixelRatio

            return centred + Math.round(sceneY * ratio) / ratio - sceneY
        }
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
                color: current ? AppTheme.colourOutline : (done ? AppTheme.colourAccent : AppTheme.colourBorder)

                Rectangle
                {
                    objectName: "stage_fill"

                    visible: parent.current
                    width: parent.height + (parent.width - parent.height) * Math.max(0, Math.min(1, root.stageProgress))
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
