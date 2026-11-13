import QtQuick
import QtQuick.Layouts

Flow
{
    id: root

    required property bool listLayout

    readonly property int columnCount: listLayout ? 1 : Math.max(2, Math.floor((width + spacing) / (AppMetrics.coverTileMinimumWidth + spacing)))
    readonly property real tileWidth: Math.max(0, Math.floor((width - spacing * (columnCount - 1)) / columnCount))

    Layout.fillWidth: true
    spacing: listLayout ? 0 : AppMetrics.gapCompact
}
