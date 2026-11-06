import QtQuick
import QtQuick.Layouts

Rectangle
{
    id: root

    default property alias content: column.data
    property alias spacing: column.spacing
    property bool dividerOnLeft: false
    property string widthKey: ""
    property int defaultWidth: AppPaneWidths.minimumWidth

    width: grip.paneWidth
    color: AppTheme.colourPanel

    DesktopDivider
    {
        x: root.dividerOnLeft ? 0 : root.width - width
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        vertical: true
    }

    DesktopPaneGrip
    {
        id: grip

        visible: root.widthKey.length > 0
        enabled: visible
        widthKey: root.widthKey
        defaultWidth: root.defaultWidth
        onLeft: root.dividerOnLeft
    }

    ColumnLayout
    {
        id: column

        anchors.fill: parent
        anchors.margins: AppMetrics.gapMedium
        anchors.leftMargin: AppMetrics.gapMedium + (root.dividerOnLeft ? AppMetrics.borderWidth : 0)
        anchors.rightMargin: AppMetrics.gapMedium + (root.dividerOnLeft ? 0 : AppMetrics.borderWidth)
        spacing: AppMetrics.gapCompact
    }
}
