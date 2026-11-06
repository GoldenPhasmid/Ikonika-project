import QtQuick
import QtQuick.Layouts

Rectangle
{
    id: root

    property bool vertical: false

    implicitWidth: root.vertical ? AppMetrics.borderWidth : 0
    implicitHeight: root.vertical ? 0 : AppMetrics.borderWidth

    Layout.fillWidth: !root.vertical
    Layout.fillHeight: root.vertical

    color: AppTheme.colourBorder
}
