import QtQuick
import QtQuick.Layouts
import "MobileIcons.js" as Icons

Rectangle
{
    id: root

    required property string label
    required property int remaining

    property color accent: AppTheme.colourAccent
    property bool interactive: true
    property bool completed: false
    property bool showRemaining: true

    signal clicked()

    readonly property color contentColour: root.interactive ? AppTheme.colourPanel : AppTheme.colourMuted

    Layout.fillWidth: true
    Layout.fillHeight: true

    radius: AppMetrics.radiusMedium
    color: root.interactive ? AppTheme.colourText : "transparent"
    border.width: root.interactive ? 0 : AppMetrics.borderWidth
    border.color: AppTheme.colourBorder
    scale: press.pressed && root.interactive ? 0.98 : 1

    Behavior on scale
    {
        NumberAnimation { duration: AppMetrics.motionFast; easing.type: AppMetrics.motionEasing }
    }

    RowLayout
    {
        anchors.fill: parent
        anchors.leftMargin: AppMetrics.contentPadding
        anchors.rightMargin: AppMetrics.contentPadding
        spacing: AppMetrics.gapSmall

        Rectangle
        {
            Layout.preferredWidth: AppMetrics.gapSmall
            Layout.preferredHeight: AppMetrics.gapSmall
            radius: width / 2
            color: root.accent
        }

        Text
        {
            Layout.fillWidth: true
            text: root.label
            font.pixelSize: AppMetrics.fontLabel
            font.weight: Font.DemiBold
            font.family: AppMetrics.fontFamily
            color: root.contentColour
            elide: Text.ElideRight
        }

        Text
        {
            visible: root.showRemaining && !root.completed
            text: root.remaining
            font.pixelSize: AppMetrics.heroFigureSize
            font.weight: Font.Bold
            font.family: AppMetrics.fontFamily
            font.features: { "tnum": 1 }
            color: root.contentColour
        }

        MobileVectorIcon
        {
            visible: root.completed
            Layout.preferredWidth: AppMetrics.heroFigureSize
            Layout.preferredHeight: AppMetrics.heroFigureSize
            svgPath: Icons.Icons.check.path
            color: root.contentColour
        }
    }

    MouseArea
    {
        id: press

        anchors.fill: parent
        enabled: root.interactive
        onClicked: root.clicked()
    }
}
