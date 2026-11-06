import QtQuick

Rectangle
{
    id: root

    property bool selected: false
    property bool hovered: false
    property bool pressed: false
    property bool focused: false
    property color hoverColour: AppTheme.colourHover
    property bool barAtBottom: false

    radius: AppMetrics.radiusMedium
    color: "transparent"

    Rectangle
    {
        anchors.fill: parent
        radius: root.radius
        color: root.hoverColour
        opacity: root.hovered && !root.selected && !root.pressed ? 1 : 0

        Behavior on opacity { NumberAnimation { duration: AppMetrics.motionFast } }
    }

    Rectangle
    {
        anchors.fill: parent
        radius: root.radius
        color: AppTheme.colourFaint
        opacity: root.selected || root.pressed ? 1 : 0

        Behavior on opacity { NumberAnimation { duration: AppMetrics.motionFast } }
    }

    Rectangle
    {
        visible: root.selected && !root.barAtBottom
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        width: AppMetrics.selectionBarWidth
        height: AppMetrics.selectionBarHeight
        radius: width / 2
        color: AppTheme.colourAccent
    }

    Rectangle
    {
        visible: root.selected && root.barAtBottom
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        width: AppMetrics.selectionBarHeight
        height: AppMetrics.selectionBarWidth
        radius: height / 2
        color: AppTheme.colourAccent
    }

    DesktopFocusRing
    {
        visible: root.focused
    }
}
