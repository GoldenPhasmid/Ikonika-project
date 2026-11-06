import QtQuick
import QtQuick.Shapes

Shape
{
    id: root

    property string svgPath: ""
    property real minX: 0
    property real minY: 0
    property real sourceWidth: 24
    property real sourceHeight: 24
    property color color: AppTheme.colourText
    property real strokeWidth: 1.5
    property bool filled: false

    readonly property real fitScale: root.width / Math.max(root.sourceWidth, root.sourceHeight)

    transform:
    [
        Translate { x: -root.minX; y: -root.minY },
        Scale { xScale: root.fitScale; yScale: root.fitScale },
        Translate
        {
            x: (root.width - root.sourceWidth * root.fitScale) / 2
            y: (root.height - root.sourceHeight * root.fitScale) / 2
        }
    ]

    ShapePath
    {
        strokeColor: root.filled ? "transparent" : root.color
        strokeWidth: root.strokeWidth
        fillColor: root.filled ? root.color : "transparent"
        capStyle: ShapePath.RoundCap
        joinStyle: ShapePath.RoundJoin

        PathSvg { path: root.svgPath }
    }
}
