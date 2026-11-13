import QtQuick
import QtQuick.Layouts

GridLayout
{
    id: root

    required property real availableWidth

    Layout.alignment: Qt.AlignHCenter
    columns: Math.max(1, Math.floor((root.availableWidth + root.columnSpacing) / (AppMetrics.languageChoiceWidth + root.columnSpacing)))
    columnSpacing: AppMetrics.gapSmall
    rowSpacing: AppMetrics.gapSmall
}
