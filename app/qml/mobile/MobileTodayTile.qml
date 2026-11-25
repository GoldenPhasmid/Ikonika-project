import QtQuick
import QtQuick.Layouts
import QtQuick.Shapes

Item
{
    id: root

    required property string label
    required property string iconSvg
    property bool locked: false
    property bool primary: false
    property bool compactPhone: false

    signal clicked()
    signal blockedClicked()

    readonly property bool filled: root.primary && !root.locked
    readonly property real cornerRadius: 18
    readonly property real wedgeBase: root.compactPhone ? 64 : 74
    readonly property real wedgeSlope: 1.695
    readonly property color contentColour: root.locked ? AppTheme.colourMuted
                                                       : root.filled ? AppTheme.colourPanel : AppTheme.colourText

    Layout.fillWidth: true
    Layout.preferredWidth: 1
    Layout.preferredHeight: root.compactPhone ? 80 : 92

    scale: tap.pressed && !root.locked ? 0.96 : 1

    Behavior on scale
    {
        NumberAnimation { duration: AppMetrics.motionFast; easing.type: AppMetrics.motionEasing }
    }

    Accessible.role: Accessible.Button
    Accessible.name: root.label
    Accessible.onPressAction: root.locked ? root.blockedClicked() : root.clicked()

    Rectangle
    {
        anchors.fill: parent
        visible: !root.locked
        radius: root.cornerRadius
        color: root.filled ? AppTheme.colourText : AppTheme.colourPanel
        border.width: root.filled ? 0 : AppMetrics.borderWidth
        border.color: AppTheme.colourBorder
    }

    DesktopDashedBox
    {
        anchors.fill: parent
        visible: root.locked
        radius: root.cornerRadius
    }

    Shape
    {
        id: wedge

        anchors.fill: parent
        anchors.margins: AppMetrics.borderWidth
        visible: !root.locked
        preferredRendererType: Shape.CurveRenderer

        readonly property real radius: root.cornerRadius - AppMetrics.borderWidth

        ShapePath
        {
            strokeWidth: -1
            fillColor: root.filled ? AppTheme.colourCoverStripe : AppTheme.colourTileWedge

            startX: wedge.width - root.wedgeBase
            startY: wedge.height

            PathLine { x: wedge.width; y: wedge.height - root.wedgeBase / root.wedgeSlope }
            PathLine { x: wedge.width; y: wedge.height - wedge.radius }
            PathArc { x: wedge.width - wedge.radius; y: wedge.height; radiusX: wedge.radius; radiusY: wedge.radius }
            PathLine { x: wedge.width - root.wedgeBase; y: wedge.height }
        }
    }

    MobileVectorIcon
    {
        x: root.compactPhone ? 12 : 14
        y: x
        width: 22
        height: 22

        svgPath: root.iconSvg
        color: root.contentColour
    }

    Text
    {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: root.compactPhone ? 12 : 14

        text: root.label
        elide: Text.ElideRight
        font.family: AppMetrics.fontFamily
        font.pixelSize: root.compactPhone ? 14 : 15
        font.weight: Font.DemiBold
        color: root.contentColour
    }

    MouseArea
    {
        id: tap

        anchors.fill: parent

        onClicked: root.locked ? root.blockedClicked() : root.clicked()
    }
}
