import QtQuick
import "MobileIcons.js" as Icons

Rectangle
{
    id: root

    required property string label

    property int minimumWidth: 112
    property bool interactive: true

    signal clicked()

    implicitWidth: Math.max(root.minimumWidth,
                            value.implicitWidth + chevron.width + 30)
    implicitHeight: 36

    radius: AppMetrics.radiusSmall
    color: press.pressed ? AppTheme.colourBorder : AppTheme.colourFaint
    opacity: root.interactive ? 1.0 : 0.5

    Behavior on color { ColorAnimation { duration: AppMetrics.motionFast } }

    Text
    {
        id: value

        anchors.left: parent.left
        anchors.leftMargin: AppMetrics.gapCompact
        anchors.verticalCenter: parent.verticalCenter

        text: root.label
        font.pixelSize: AppMetrics.fontLabel
        font.weight: Font.Medium
        font.family: AppMetrics.fontFamily
        color: AppTheme.colourText
    }

    MobileVectorIcon
    {
        id: chevron

        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.verticalCenter: parent.verticalCenter

        width: 12
        height: 12

        sourceWidth: Icons.Icons.chevron_down.width
        sourceHeight: Icons.Icons.chevron_down.height
        svgPath: Icons.Icons.chevron_down.path
        color: AppTheme.colourSubtle
    }

    MouseArea
    {
        id: press

        anchors.fill: parent
        anchors.margins: -4
        enabled: root.interactive
        onClicked: root.clicked()
    }
}
