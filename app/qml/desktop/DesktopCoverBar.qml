import QtQuick

Rectangle
{
    id: root

    default property alias content: row.data
    property bool shown: true

    opacity: root.shown ? 1 : 0
    visible: root.opacity > 0

    Behavior on opacity { NumberAnimation { duration: AppMetrics.motionFast } }

    implicitWidth: row.implicitWidth + AppMetrics.gapTight * 2
    implicitHeight: row.implicitHeight + AppMetrics.gapTight * 2
    radius: AppMetrics.radiusMedium
    color: AppTheme.colourText
    border.width: AppMetrics.borderWidth
    border.color: AppTheme.faded(AppTheme.colourPanel, AppTheme.alphaStrong)

    Row
    {
        id: row

        anchors.centerIn: parent
        spacing: AppMetrics.gapTight
    }
}
