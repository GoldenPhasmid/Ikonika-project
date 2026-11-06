import QtQuick

Rectangle
{
    id: root

    property real progress: 0
    property color fill: AppTheme.colourText
    property color track: AppTheme.colourFaint
    property bool animated: true

    readonly property real clamped: Math.max(0, Math.min(1, root.progress))

    implicitHeight: AppMetrics.progressBarHeight

    radius: height / 2
    color: root.track

    Accessible.role: Accessible.ProgressBar
    Accessible.name: qsTr("%1 percent").arg(Math.round(root.clamped * 100))

    Rectangle
    {
        width: parent.width * root.clamped
        height: parent.height
        radius: parent.radius
        color: root.fill

        Behavior on width
        {
            enabled: root.animated

            NumberAnimation { duration: AppMetrics.motionSlow; easing.type: AppMetrics.motionEasing }
        }
    }
}
