import QtQuick
import QtQuick.Layouts

ColumnLayout
{
    id: root

    property string title: ""
    property color outlineColour: AppTheme.colourBorder
    property real outlineWidth: AppMetrics.borderWidth
    property bool squareTopLeft: false
    property bool squareTopRight: false
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
        topLeftRadius: root.squareTopLeft ? 0 : AppMetrics.radiusLarge
        topRightRadius: root.squareTopRight ? 0 : AppMetrics.radiusLarge
        color: AppTheme.colourPanel
        border.width: root.outlineWidth
        border.color: root.outlineColour

        ColumnLayout
        {
            id: rowColumn

            anchors.left: parent.left
            anchors.right: parent.right
            spacing: 0
        }
    }
}
