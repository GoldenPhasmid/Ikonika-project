import QtQuick
import QtQuick.Layouts

Rectangle
{
    id: root

    default property alias content: rows.data

    Layout.fillWidth: true
    implicitHeight: rows.implicitHeight

    radius: AppMetrics.radiusMedium
    color: AppTheme.colourPanel
    border.color: AppTheme.colourBorder
    border.width: AppMetrics.borderWidth
    clip: true

    ColumnLayout
    {
        id: rows

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: 0
    }
}
