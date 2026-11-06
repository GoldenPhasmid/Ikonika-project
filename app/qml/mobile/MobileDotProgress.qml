import QtQuick
import QtQuick.Layouts

Item
{
    id: root

    property real progress: 0

    property real shownProgress: Math.max(0, Math.min(1, root.progress))
    property bool settled: false

    Layout.fillWidth: true
    Layout.preferredHeight: AppMetrics.progressBarHeight

    Component.onCompleted: Qt.callLater(function() { root.settled = true })

    Behavior on shownProgress
    {
        enabled: root.settled

        NumberAnimation { duration: AppMetrics.motionSlow; easing.type: AppMetrics.motionEasing }
    }

    Rectangle
    {
        anchors.fill: parent
        radius: AppMetrics.progressBarHeight / 2
        color: AppTheme.colourFaint
    }

    Rectangle
    {
        width: parent.width * root.shownProgress
        height: parent.height
        radius: AppMetrics.progressBarHeight / 2
        color: AppTheme.colourText
    }
}
