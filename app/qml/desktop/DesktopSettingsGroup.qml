import QtQuick
import QtQuick.Layouts

ColumnLayout
{
    id: root

    property string title: ""
    default property alias rows: rowColumn.data

    Layout.fillWidth: true
    spacing: AppMetrics.gapSmall

    DesktopSectionLabel
    {
        visible: root.title.length > 0
        text: root.title
    }

    Rectangle
    {
        Layout.fillWidth: true
        implicitHeight: rowColumn.implicitHeight

        radius: AppMetrics.radiusLarge
        color: AppTheme.colourPanel
        border.width: AppMetrics.borderWidth
        border.color: AppTheme.colourBorder

        ColumnLayout
        {
            id: rowColumn

            anchors.left: parent.left
            anchors.right: parent.right
            spacing: 0
        }
    }
}
