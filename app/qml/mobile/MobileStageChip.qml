import QtQuick
import QtQuick.Layouts
import QtQuick.Shapes
import "MobileIcons.js" as Icons

Rectangle
{
    id: root

    required property string label
    required property string iconName

    property bool on: false
    property bool selected: false
    property bool locked: false

    signal clicked()

    readonly property real stroke: AppMetrics.borderWidth * 1.5
    readonly property real inset: root.stroke / 2
    readonly property real cornerRadius: AppMetrics.radiusMedium

    implicitHeight: AppMetrics.gapLarge * 2

    radius: root.cornerRadius
    bottomLeftRadius: root.selected ? 0 : root.cornerRadius
    bottomRightRadius: root.selected ? 0 : root.cornerRadius
    color:
    {
        if (root.locked)
        {
            return AppTheme.colourFaint
        }

        if (root.selected)
        {
            return AppTheme.colourPanel
        }

        if (root.on)
        {
            return Qt.tint(AppTheme.colourPanel, AppTheme.faded(AppTheme.colourAccent, AppTheme.alphaSoft))
        }

        return "transparent"
    }
    border.width: (root.on || root.selected) && !root.locked ? root.stroke : 0
    border.color: AppTheme.colourAccent

    Accessible.role: Accessible.PageTab
    Accessible.name: root.label
    Accessible.selected: root.selected

    Shape
    {
        anchors.fill: parent
        visible: !root.on && !root.selected && !root.locked
        preferredRendererType: Shape.CurveRenderer

        ShapePath
        {
            strokeColor: AppTheme.colourBorder
            strokeWidth: root.stroke
            fillColor: "transparent"
            strokeStyle: ShapePath.DashLine
            dashPattern: [2, 2]
            joinStyle: ShapePath.RoundJoin

            PathMove { x: root.cornerRadius; y: root.inset }
            PathLine { x: root.width - root.cornerRadius; y: root.inset }
            PathArc
            {
                x: root.width - root.inset; y: root.cornerRadius
                radiusX: root.cornerRadius; radiusY: root.cornerRadius
            }
            PathLine { x: root.width - root.inset; y: root.height - root.cornerRadius }
            PathArc
            {
                x: root.width - root.cornerRadius; y: root.height - root.inset
                radiusX: root.cornerRadius; radiusY: root.cornerRadius
            }
            PathLine { x: root.cornerRadius; y: root.height - root.inset }
            PathArc
            {
                x: root.inset; y: root.height - root.cornerRadius
                radiusX: root.cornerRadius; radiusY: root.cornerRadius
            }
            PathLine { x: root.inset; y: root.cornerRadius }
            PathArc
            {
                x: root.cornerRadius; y: root.inset
                radiusX: root.cornerRadius; radiusY: root.cornerRadius
            }
        }
    }

    MobileVectorIcon
    {
        anchors.centerIn: parent
        width: AppMetrics.iconSize
        height: AppMetrics.iconSize
        svgPath: Icons.Icons[root.iconName].path
        color: root.on && !root.locked ? AppTheme.colourAccent : AppTheme.colourMuted
    }

    MobileVectorIcon
    {
        visible: root.locked
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: AppMetrics.gapTight
        width: AppMetrics.gapCompact
        height: AppMetrics.gapCompact
        svgPath: Icons.Icons.lock.path
        color: AppTheme.colourMuted
    }

    MouseArea
    {
        anchors.fill: parent
        enabled: !root.locked

        onClicked: root.clicked()
    }
}
