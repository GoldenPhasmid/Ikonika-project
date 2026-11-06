import QtQuick
import QtQuick.Shapes

Shape
{
    id: root

    property color colour: AppTheme.colourCoverStripe

    anchors.fill: parent

    preferredRendererType: Shape.CurveRenderer

    ShapePath
    {
        strokeWidth: -1
        fillColor: root.colour

        startX: width * AppMetrics.coverStripeBottomStart
        startY: height

        PathLine { x: width * AppMetrics.coverStripeBottomEnd; y: height }
        PathLine { x: width; y: height * AppMetrics.coverStripeRightEnd }
        PathLine { x: width; y: height * AppMetrics.coverStripeRightStart }
        PathLine { x: width * AppMetrics.coverStripeBottomStart; y: height }
    }
}
