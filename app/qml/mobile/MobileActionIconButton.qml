import QtQuick
import QtQuick.Layouts

Rectangle
{
    id: root

    property string icon: ""
    property string iconSvg: ""
    property real iconMinX: 0
    property real iconMinY: 0
    property real iconWidth: 24
    property real iconHeight: 24

    readonly property real iconBoxSize: 18

    property bool interactive: true

    signal clicked()

    Layout.preferredWidth: 32
    Layout.preferredHeight: 32

    radius: AppMetrics.radiusSmall
    color: AppTheme.colourFaint
    opacity: root.interactive ? 1.0 : 0.5

    Text
    {
        anchors.centerIn: parent

        visible: root.iconSvg.length === 0
        text: root.icon
        font.pixelSize: AppMetrics.fontBody
        color: AppTheme.colourText
    }

    MobileVectorIcon
    {
        anchors.centerIn: parent

        visible: root.iconSvg.length > 0
        width: root.iconBoxSize
        height: root.iconBoxSize

        minX: root.iconMinX
        minY: root.iconMinY
        sourceWidth: root.iconWidth
        sourceHeight: root.iconHeight
        svgPath: root.iconSvg
        color: AppTheme.colourText
    }

    MouseArea
    {
        anchors.fill: parent
        enabled: root.interactive
        onClicked: root.clicked()
    }
}
