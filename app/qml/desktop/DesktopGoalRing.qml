import QtQuick
import QtQuick.Layouts
import QtQuick.Shapes

Item
{
    id: root

    default property alias content: centre.data
    property real progress: 0
    property color fill: AppTheme.colourText
    property color track: AppTheme.colourBorder
    property int sweepDuration: AppMetrics.motionSlow
    property real stroke: AppMetrics.goalRingStroke
    property bool showRing: true

    readonly property real clamped: Math.max(0, Math.min(1, root.progress))
    readonly property real ringRadius: (root.width - root.stroke) / 2

    implicitWidth: AppMetrics.goalRingSize
    implicitHeight: AppMetrics.goalRingSize

    Accessible.role: Accessible.ProgressBar
    Accessible.name: qsTr("%1 percent").arg(Math.round(root.clamped * 100))

    Shape
    {
        anchors.fill: parent
        visible: root.showRing
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
                id: arc

                centerX: root.width / 2
                centerY: root.height / 2
                radiusX: root.ringRadius
                radiusY: root.ringRadius
                startAngle: -90
                sweepAngle: 360 * root.clamped
                moveToStart: true

                Behavior on sweepAngle { NumberAnimation { duration: root.sweepDuration; easing.type: AppMetrics.motionEasing } }
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
