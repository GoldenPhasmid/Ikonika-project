import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Flickable
{
    id: root

    default property alias content: column.data
    property alias spacing: column.spacing
    property real maximumWidth: AppMetrics.contentWidthWide
    property bool centred: false
    property real topSpacing: AppMetrics.contentPadding
    property real topFill: root.centred ? 0.5 : 0
    readonly property alias columnWidth: column.width

    contentWidth: width
    contentHeight: column.y + column.implicitHeight + AppMetrics.contentPadding
    boundsBehavior: Flickable.StopAtBounds
    clip: true

    ScrollBar.vertical: DesktopScrollBar {}

    ColumnLayout
    {
        id: column

        x: (root.width - width) / 2
        y: Math.max(root.topSpacing, (root.height - column.implicitHeight) * root.topFill)
        width: Math.min(root.maximumWidth, root.width - AppMetrics.contentPadding * 2)
        spacing: AppMetrics.gapLarge
    }
}
