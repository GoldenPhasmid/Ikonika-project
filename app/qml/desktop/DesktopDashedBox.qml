import QtQuick
import QtQuick.Shapes

Shape
{
    id: root

    property color colour: AppTheme.colourBorder
    property real radius: AppMetrics.radiusMedium
    property real stroke: AppMetrics.borderWidth

    preferredRendererType: Shape.CurveRenderer

    ShapePath
    {
        strokeColor: root.colour
        strokeWidth: root.stroke
        strokeStyle: ShapePath.DashLine
        dashPattern: [4, 3]
        fillColor: "transparent"

        PathRectangle
        {
            x: root.stroke / 2
            y: root.stroke / 2
            width: root.width - root.stroke
            height: root.height - root.stroke
            radius: root.radius
        }
    }
}
