import QtQuick
import QtQuick.Layouts
import QtQuick.Shapes

Rectangle
{
    id: root

    required property string label

    property string iconSvg: ""
    property int signalLevel: 0
    property int signalBars: 3
    property bool checked: false

    signal clicked()

    readonly property real dashStrokeWidth: AppMetrics.borderWidth * 1.5
    readonly property real dashInset: root.dashStrokeWidth / 2
    readonly property real dashRadius: root.radius

    Layout.fillWidth: true
    Layout.preferredHeight: 66

    radius: AppMetrics.radiusMedium
    color: root.checked
        ? Qt.tint(AppTheme.colourPanel, Qt.rgba(AppTheme.colourAccent.r,
              AppTheme.colourAccent.g, AppTheme.colourAccent.b, 0.12))
        : "transparent"
    border.width: root.checked ? root.dashStrokeWidth : 0
    border.color: AppTheme.colourAccent

    Shape
    {
        anchors.fill: parent
        visible: !root.checked
        preferredRendererType: Shape.CurveRenderer

        ShapePath
        {
            strokeColor: AppTheme.colourBorder
            strokeWidth: root.dashStrokeWidth
            fillColor: "transparent"
            strokeStyle: ShapePath.DashLine
            dashPattern: [2, 2]
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

    ColumnLayout
    {
        anchors.centerIn: parent
        spacing: AppMetrics.gapTight

        MobileVectorIcon
        {
            visible: root.iconSvg.length > 0

            Layout.alignment: Qt.AlignHCenter

            width: 20
            height: 20
            svgPath: root.iconSvg
            color: root.checked ? AppTheme.colourAccent : AppTheme.colourMuted
        }

        MobileSignalBars
        {
            visible: root.signalLevel > 0

            Layout.alignment: Qt.AlignHCenter

            level: root.signalLevel
            bars: root.signalBars
            activeColor: root.checked ? AppTheme.colourAccent : AppTheme.colourMuted
        }

        Text
        {
            Layout.alignment: Qt.AlignHCenter

            text: root.label
            font.pixelSize: AppMetrics.fontMeta
            font.weight: Font.Medium
            font.family: AppMetrics.fontFamily
            color: root.checked ? AppTheme.colourText : AppTheme.colourMuted
        }
    }

    MouseArea
    {
        anchors.fill: parent
        onClicked: root.clicked()
    }
}
