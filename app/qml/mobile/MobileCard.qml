import QtQuick
import QtQuick.Layouts

Rectangle
{
    id: root

    default property alias content: contentColumn.data
    property alias spacing: contentColumn.spacing

    Layout.fillWidth: true
    implicitHeight: contentColumn.implicitHeight + AppMetrics.contentPadding * 2
    height: implicitHeight

    radius: AppMetrics.radiusMedium
    color: AppTheme.colourPanel
    border.width: AppMetrics.borderWidth
    border.color: AppTheme.colourBorder

    ColumnLayout
    {
        id: contentColumn

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: AppMetrics.contentPadding
        spacing: AppMetrics.gapSmall
    }
}
