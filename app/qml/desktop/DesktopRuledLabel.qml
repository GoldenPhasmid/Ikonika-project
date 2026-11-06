import QtQuick
import QtQuick.Layouts

RowLayout
{
    id: root

    property alias text: label.text
    property alias trailing: trailingLabel.text

    Layout.fillWidth: true
    spacing: AppMetrics.gapCompact

    DesktopSectionLabel
    {
        id: label
    }

    DesktopDivider
    {
        Layout.alignment: Qt.AlignVCenter
    }

    DesktopSectionLabel
    {
        id: trailingLabel

        visible: text.length > 0
    }
}
