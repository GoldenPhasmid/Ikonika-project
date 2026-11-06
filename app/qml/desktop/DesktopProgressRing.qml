import QtQuick
import QtQuick.Shapes

Item
{
    id: root

    property int stage: 0
    property int stageCount: 1
    property bool completed: false

    readonly property int segments: Math.max(1, root.stageCount)
    readonly property real gapDegrees: root.segments > 1 ? 36 : 0
    readonly property real segmentSpan: 360 / root.segments - root.gapDegrees
    readonly property real ringRadius: (root.width - AppMetrics.progressRingStroke) / 2

    implicitWidth: AppMetrics.progressRingSize
    implicitHeight: AppMetrics.progressRingSize

    Accessible.role: Accessible.Indicator
    Accessible.name: root.completed ? qsTr("Learned")
                                    : qsTr("Interval %1 of %2").arg(root.stage).arg(root.segments)

    Repeater
    {
        model: root.completed ? 1 : root.segments

        delegate: Shape
        {
            required property int index

            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer

            ShapePath
            {
                strokeColor: root.completed ? AppTheme.colourCorrect
                                            : (index < root.stage ? AppTheme.colourAccent : AppTheme.faded(AppTheme.colourAccent, AppTheme.alphaHalf))
                strokeWidth: AppMetrics.progressRingStroke
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap

                PathAngleArc
                {
                    centerX: root.width / 2
                    centerY: root.height / 2
                    radiusX: root.ringRadius
                    radiusY: root.ringRadius
                    startAngle: root.completed ? -90 : -90 + index * (360 / root.segments) + root.gapDegrees / 2
                    sweepAngle: root.completed ? 360 : root.segmentSpan
                    moveToStart: true
                }
            }
        }
    }
}
