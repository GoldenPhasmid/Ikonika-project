import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Shapes

Button
{
    id: root

    property string iconName: ""
    property bool on: false

    readonly property real dashStrokeWidth: AppMetrics.dashStrokeWidth
    readonly property real dashInset: root.dashStrokeWidth / 2
    readonly property real dashRadius: AppMetrics.radiusMedium

    Layout.fillWidth: true
    implicitHeight: AppMetrics.toggleChipHeight

    padding: 0
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus

    Accessible.role: Accessible.CheckBox
    Accessible.name: root.text
    Accessible.checkable: true
    Accessible.checked: root.on

    contentItem: ColumnLayout
    {
        spacing: AppMetrics.gapTight

        Item { Layout.fillHeight: true }

        DesktopIcon
        {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: AppMetrics.iconSize
            Layout.preferredHeight: AppMetrics.iconSize
            visible: root.iconName.length > 0
            name: root.iconName
            color: root.on ? AppTheme.colourAccent : AppTheme.colourMuted
        }

        DesktopLabel
        {
            Layout.alignment: Qt.AlignHCenter
            text: root.text
            font.pixelSize: AppMetrics.fontSmall
            font.weight: Font.Medium
            color: root.on ? AppTheme.colourText : AppTheme.colourMuted
        }

        Item { Layout.fillHeight: true }
    }

    background: Rectangle
    {
        radius: root.dashRadius
        color:
        {
            if (root.on)
            {
                return Qt.tint(AppTheme.colourPanel, AppTheme.faded(AppTheme.colourAccent,
                                                                    root.hovered ? AppTheme.alphaMedium : AppTheme.alphaSoft))
            }

            return root.hovered ? AppTheme.colourHover : AppTheme.faded(AppTheme.colourHover, 0)
        }
        border.width: root.on ? root.dashStrokeWidth : 0
        border.color: AppTheme.colourAccent

        Behavior on color { ColorAnimation { duration: AppMetrics.motionFast } }

        Shape
        {
            anchors.fill: parent
            visible: !root.on
            preferredRendererType: Shape.CurveRenderer

            ShapePath
            {
                strokeColor: root.hovered ? AppTheme.colourOutline : AppTheme.colourBorder
                strokeWidth: root.dashStrokeWidth
                fillColor: "transparent"
                strokeStyle: ShapePath.DashLine
                dashPattern: [AppMetrics.dashLength, AppMetrics.dashLength]
                joinStyle: ShapePath.RoundJoin

                PathMove { x: root.dashRadius; y: root.dashInset }
                PathLine { x: root.width - root.dashRadius; y: root.dashInset }
                PathArc
                {
                    x: root.width - root.dashInset; y: root.dashRadius
                    radiusX: root.dashRadius; radiusY: root.dashRadius
                }
                PathLine { x: root.width - root.dashInset; y: root.height - root.dashRadius }
                PathArc
                {
                    x: root.width - root.dashRadius; y: root.height - root.dashInset
                    radiusX: root.dashRadius; radiusY: root.dashRadius
                }
                PathLine { x: root.dashRadius; y: root.height - root.dashInset }
                PathArc
                {
                    x: root.dashInset; y: root.height - root.dashRadius
                    radiusX: root.dashRadius; radiusY: root.dashRadius
                }
                PathLine { x: root.dashInset; y: root.dashRadius }
                PathArc
                {
                    x: root.dashRadius; y: root.dashInset
                    radiusX: root.dashRadius; radiusY: root.dashRadius
                }
            }
        }

        DesktopFocusRing
        {
            visible: root.visualFocus
            baseRadius: root.dashRadius
        }
    }

    HoverHandler
    {
        cursorShape: Qt.PointingHandCursor
    }
}
