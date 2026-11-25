import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Shapes

Button
{
    id: root

    property string iconName: ""
    property bool on: false
    property bool selected: false
    property bool locked: false

    readonly property bool emphasised: root.hovered && !root.locked && !root.selected

    readonly property real dashStrokeWidth: AppMetrics.dashStrokeWidth
    readonly property real dashInset: root.dashStrokeWidth / 2
    readonly property real dashRadius: AppMetrics.radiusMedium

    Layout.fillWidth: true
    implicitHeight: AppMetrics.toggleChipHeight

    padding: 0
    leftPadding: root.locked ? AppMetrics.gapMedium : 0
    rightPadding: root.locked ? AppMetrics.gapMedium : 0
    hoverEnabled: true
    focusPolicy: root.locked ? Qt.NoFocus : Qt.StrongFocus

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
            color: root.on && !root.locked ? AppTheme.colourAccent : AppTheme.colourMuted
        }

        RowLayout
        {
            Layout.alignment: Qt.AlignHCenter
            spacing: AppMetrics.gapTight

            DesktopIcon
            {
                visible: root.locked
                Layout.preferredWidth: AppMetrics.iconSizeSmall
                Layout.preferredHeight: AppMetrics.iconSizeSmall
                name: "lock"
                color: AppTheme.colourMuted
            }

            DesktopLabel
            {
                text: root.text
                font.pixelSize: AppMetrics.fontSmall
                font.weight: Font.Medium
                color: root.locked ? AppTheme.colourSubtle : root.on ? AppTheme.colourText : AppTheme.colourMuted
            }
        }

        Item { Layout.fillHeight: true }
    }

    background: Rectangle
    {
        radius: root.dashRadius
        bottomLeftRadius: root.selected ? 0 : root.dashRadius
        bottomRightRadius: root.selected ? 0 : root.dashRadius
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
                return Qt.tint(AppTheme.colourPanel, AppTheme.faded(AppTheme.colourAccent,
                                                                    root.emphasised ? AppTheme.alphaMedium : AppTheme.alphaSoft))
            }

            return root.emphasised ? AppTheme.colourHover : AppTheme.faded(AppTheme.colourHover, 0)
        }
        border.width: (root.on || root.selected) && !root.locked ? root.dashStrokeWidth : 0
        border.color: AppTheme.colourAccent

        Behavior on color { ColorAnimation { duration: AppMetrics.motionFast } }

        Shape
        {
            anchors.fill: parent
            visible: !root.on && !root.selected && !root.locked
            preferredRendererType: Shape.CurveRenderer

            ShapePath
            {
                strokeColor: root.emphasised ? AppTheme.colourOutline : AppTheme.colourBorder
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
        cursorShape: root.locked ? Qt.ArrowCursor : Qt.PointingHandCursor
    }
}
