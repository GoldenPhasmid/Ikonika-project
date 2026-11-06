import QtQuick
import QtQuick.Layouts

ColumnLayout
{
    id: root

    property string label: ""
    property string trailingLabel: ""
    property string hint: ""
    default property alias field: fieldSlot.data

    Layout.fillWidth: true
    spacing: AppMetrics.gapSmall

    RowLayout
    {
        Layout.fillWidth: true
        spacing: AppMetrics.gapSmall

        DesktopLabel
        {
            Layout.fillWidth: true
            text: root.label
            font.pixelSize: AppMetrics.fontSmall
            font.weight: Font.DemiBold
            color: AppTheme.colourSubtle
        }

        DesktopLabel
        {
            visible: root.trailingLabel.length > 0
            text: root.trailingLabel
            font.pixelSize: AppMetrics.fontSmall
            color: AppTheme.colourMuted
        }
    }

    ColumnLayout
    {
        id: fieldSlot

        Layout.fillWidth: true
        spacing: 0
    }

    DesktopLabel
    {
        Layout.fillWidth: true
        visible: root.hint.length > 0
        text: root.hint
        font.pixelSize: AppMetrics.fontSmall
        color: AppTheme.colourMuted
        wrapMode: Text.WordWrap
    }
}
