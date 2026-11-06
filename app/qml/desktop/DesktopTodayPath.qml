import QtQuick
import QtQuick.Shapes

Item
{
    id: root

    property real ringRight: 0
    property real ringY: 0
    property real reviewY: 0
    property real learnY: 0
    property real grammarY: 0
    property real tileLeft: 0
    property real grammarLeft: 0
    property real practiceArrowX: 0
    property bool hasGrammar: true
    property bool reviewDone: false
    property bool learnDone: false
    property bool learnCurrent: false

    readonly property real lineX: root.ringRight + AppMetrics.todayBracketOffset
    readonly property real nodeRadius: AppMetrics.pathNodeSize / 2
    readonly property real bend: AppMetrics.radiusLarge
    readonly property real dashEnd: grammarArrow.x + grammarArrow.width / 2

    function lit(on)
    {
        return on ? AppTheme.colourText : AppTheme.colourOutline
    }

    Shape
    {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath
        {
            strokeColor: AppTheme.colourText
            strokeWidth: AppMetrics.pathLineWidth
            fillColor: "transparent"

            startX: root.ringRight + AppMetrics.gapTight
            startY: root.ringY

            PathLine { x: root.lineX; y: root.ringY }
        }

        ShapePath
        {
            strokeColor: root.lit(root.reviewDone)
            strokeWidth: AppMetrics.pathLineWidth
            fillColor: "transparent"

            startX: root.lineX
            startY: root.ringY

            PathLine { x: root.lineX; y: root.reviewY + root.nodeRadius }
        }

        ShapePath
        {
            strokeColor: root.lit(root.learnDone)
            strokeWidth: AppMetrics.pathLineWidth
            fillColor: "transparent"

            startX: root.lineX
            startY: root.ringY

            PathLine { x: root.lineX; y: root.learnY - root.nodeRadius }
        }

        ShapePath
        {
            strokeColor: AppTheme.colourText
            strokeWidth: AppMetrics.pathLineWidth
            fillColor: "transparent"

            startX: root.lineX + root.nodeRadius
            startY: root.reviewY

            PathLine { x: root.tileLeft; y: root.reviewY }
        }

        ShapePath
        {
            strokeColor: root.lit(root.learnDone || root.learnCurrent)
            strokeWidth: AppMetrics.pathLineWidth
            fillColor: "transparent"

            startX: root.lineX + root.nodeRadius
            startY: root.learnY

            PathLine { x: root.tileLeft; y: root.learnY }
        }

        ShapePath
        {
            strokeColor: root.hasGrammar ? root.lit(root.learnDone) : "transparent"
            strokeWidth: AppMetrics.pathLineWidth
            strokeStyle: ShapePath.DashLine
            dashPattern: [1.5, 2.5]
            fillColor: "transparent"

            startX: root.lineX
            startY: root.learnY + root.nodeRadius + AppMetrics.gapTight

            PathLine { x: root.lineX; y: root.grammarY - root.bend }
            PathQuad { x: root.lineX + root.bend; y: root.grammarY; controlX: root.lineX; controlY: root.grammarY }
            PathLine { x: root.dashEnd; y: root.grammarY }
        }
    }

    DesktopPathNode
    {
        x: root.lineX - width / 2
        y: root.reviewY - height / 2

        reached: root.reviewDone
        current: !root.reviewDone
    }

    DesktopPathNode
    {
        x: root.lineX - width / 2
        y: root.learnY - height / 2

        reached: root.learnDone
        current: root.learnCurrent
    }

    DesktopIcon
    {
        id: grammarArrow

        visible: root.hasGrammar
        x: root.grammarLeft - AppMetrics.gapTight - width
        y: root.grammarY - height / 2
        name: "chevron_right"
        color: root.lit(root.learnDone)
    }

    DesktopIcon
    {
        visible: root.hasGrammar
        x: root.practiceArrowX - width / 2
        y: root.grammarY - height / 2
        name: "arrow_right"
        color: AppTheme.colourOutline
    }
}
