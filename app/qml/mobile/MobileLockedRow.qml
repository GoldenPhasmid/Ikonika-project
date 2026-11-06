import QtQuick
import QtQuick.Layouts
import "MobileIcons.js" as Icons

RowLayout
{
    id: root

    required property string label
    required property string reason

    Layout.fillWidth: true
    spacing: AppMetrics.gapCompact

    MobileVectorIcon
    {
        Layout.preferredWidth: 16
        Layout.preferredHeight: 16
        Layout.alignment: Qt.AlignVCenter
        svgPath: Icons.Icons.lock.path
        color: AppTheme.colourMuted
    }

    ColumnLayout
    {
        Layout.fillWidth: true
        spacing: 2

        Text
        {
            Layout.fillWidth: true
            text: root.label
            font.pixelSize: AppMetrics.fontLabel
            font.weight: Font.Medium
            font.family: AppMetrics.fontFamily
            color: AppTheme.colourMuted
        }

        Text
        {
            Layout.fillWidth: true
            text: root.reason
            font.pixelSize: AppMetrics.fontMeta
            font.family: AppMetrics.fontFamily
            color: AppTheme.colourMuted
            wrapMode: Text.WordWrap
        }
    }
}
