import QtQuick

Rectangle
{
    id: root

    property bool reached: false
    property bool current: false

    width: AppMetrics.pathNodeSize
    height: width
    radius: width / 2
    color: root.reached ? AppTheme.colourText : "transparent"
    border.width: root.reached ? 0 : AppMetrics.pathLineWidth
    border.color: root.current ? AppTheme.colourAccent : AppTheme.colourOutline

    DesktopIcon
    {
        anchors.centerIn: parent
        visible: root.reached
        width: parent.width - AppMetrics.gapTight
        height: width
        name: "check"
        color: AppTheme.colourPanel
    }
}
