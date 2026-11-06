import QtQuick
import QtQuick.Layouts

Rectangle
{
    id: root

    default property alias content: contentColumn.data
    property alias spacing: contentColumn.spacing
    property real padding: AppMetrics.cardPadding

    implicitWidth: contentColumn.implicitWidth + root.padding * 2
    implicitHeight: contentColumn.implicitHeight + root.padding * 2

    radius: AppMetrics.radiusLarge
    color: AppTheme.colourPanel
    border.width: AppMetrics.borderWidth
    border.color: AppTheme.colourBorder

    ColumnLayout
    {
        id: contentColumn

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: root.padding
        spacing: AppMetrics.gapMedium
    }
}
