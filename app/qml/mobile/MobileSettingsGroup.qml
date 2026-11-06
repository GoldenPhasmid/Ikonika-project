import QtQuick
import QtQuick.Layouts

ColumnLayout
{
    id: root

    required property string title

    default property alias rows: card.data

    Layout.fillWidth: true
    spacing: AppMetrics.gapSmall

    MobileSectionLabel { text: root.title }

    Rectangle
    {
        Layout.fillWidth: true
        implicitHeight: card.implicitHeight

        radius: AppMetrics.radiusMedium
        color: AppTheme.colourPanel
        border.color: AppTheme.colourBorder
        border.width: AppMetrics.borderWidth
        clip: true

        ColumnLayout
        {
            id: card

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            spacing: 0
        }
    }
}
