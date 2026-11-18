import QtQuick
import QtQuick.Layouts

Item
{
    id: root

    property bool lastRow: false

    default property alias control: controlRow.data

    Layout.fillWidth: true
    implicitHeight: Math.max(60, controlRow.implicitHeight + 24)

    RowLayout
    {
        id: controlRow

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: AppMetrics.contentPadding
        anchors.rightMargin: AppMetrics.contentPadding
        anchors.verticalCenter: parent.verticalCenter

        spacing: AppMetrics.gapSmall
    }

    Rectangle
    {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.leftMargin: AppMetrics.contentPadding

        width: parent.width - AppMetrics.contentPadding
        height: AppMetrics.borderWidth

        visible: !root.lastRow
        color: AppTheme.colourBorder
    }
}
