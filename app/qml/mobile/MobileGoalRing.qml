import QtQuick
import QtQuick.Layouts
import QtQuick.Shapes

Item
{
    id: root

    default property alias content: centre.data
    property real progress: 0
    property color fill: AppTheme.colourText
    property color track: AppTheme.colourFaint
    property real stroke: AppMetrics.goalRingStroke

    readonly property real clamped: Math.max(0, Math.min(1, root.progress))
    readonly property real ringRadius: (root.width - root.stroke) / 2

    implicitWidth: AppMetrics.goalRingSize
    implicitHeight: AppMetrics.goalRingSize

    Shape
    {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath
        {
            strokeColor: root.track
            strokeWidth: root.stroke
            fillColor: "transparent"

            PathAngleArc
            {
                centerX: root.width / 2
                centerY: root.height / 2
                radiusX: root.ringRadius
                radiusY: root.ringRadius
                startAngle: 0
                sweepAngle: 360
                moveToStart: true
            }
        }

        ShapePath
        {
            strokeColor: root.clamped > 0 ? root.fill : "transparent"
            strokeWidth: root.stroke
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap

            PathAngleArc
            {
                centerX: root.width / 2
                centerY: root.height / 2
                radiusX: root.ringRadius
                radiusY: root.ringRadius
                startAngle: -90
                sweepAngle: 360 * root.clamped
                moveToStart: true
            }
        }
    }

    ColumnLayout
    {
        id: centre

        anchors.centerIn: parent
        spacing: 0
    }
}
