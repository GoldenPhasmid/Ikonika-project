import QtQuick
import QtQuick.Shapes

Shape
{
    id: root

    property color colour: AppTheme.colourBorder
    property real stroke: AppMetrics.borderWidth

    preferredRendererType: Shape.CurveRenderer

    ShapePath
    {
        strokeColor: root.colour
        strokeWidth: root.stroke
        strokeStyle: ShapePath.DashLine
        dashPattern: [4, 3]
        fillColor: "transparent"

        PathAngleArc
        {
            centerX: root.width / 2
            centerY: root.height / 2
            radiusX: (root.width - root.stroke) / 2
            radiusY: (root.height - root.stroke) / 2
            startAngle: 0
            sweepAngle: 360
            moveToStart: true
        }
    }
}
