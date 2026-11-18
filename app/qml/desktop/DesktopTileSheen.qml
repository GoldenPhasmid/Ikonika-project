import QtQuick
import QtQuick.Shapes

Item
{
    id: root

    property color colour: "transparent"
    property real radius: 0

    readonly property real cornerRadius: root.radius - AppMetrics.borderWidth

    anchors.fill: parent
    anchors.margins: AppMetrics.borderWidth
    visible: root.colour.a > 0

    ShaderEffect
    {
        readonly property color startColour: Qt.lighter(root.colour, AppTheme.tileSheenLift)
        readonly property color endColour: root.colour
        readonly property size itemSize: Qt.size(width, height)
        readonly property real cornerRadius: root.cornerRadius

        anchors.fill: parent
        fragmentShader: "qrc:/shaders/TileSheen.frag.qsb"
    }

    Shape
    {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath
        {
            strokeWidth: -1
            fillRule: ShapePath.OddEvenFill
            fillGradient: LinearGradient
            {
                x1: 0
                y1: 0
                x2: 0
                y2: root.cornerRadius

                GradientStop { position: 0; color: Qt.lighter(root.colour, AppTheme.tileRimLift) }
                GradientStop { position: 1; color: AppTheme.faded(root.colour, 0) }
            }

            PathRectangle { width: root.width; height: root.height; radius: root.cornerRadius }
            PathRectangle { y: AppMetrics.borderWidth; width: root.width; height: root.height; radius: root.cornerRadius }
        }
    }
}
