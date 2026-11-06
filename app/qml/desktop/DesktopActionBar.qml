import QtQuick
import QtQuick.Layouts

Rectangle
{
    id: root

    default property alias actions: actionRow.data

    implicitHeight: AppMetrics.buttonHeightLarge + AppMetrics.gapMedium * 2
    color: AppTheme.colourPanel

    DesktopDivider
    {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
    }

    RowLayout
    {
        id: actionRow

        anchors.centerIn: parent
        spacing: AppMetrics.gapMedium
    }
}
