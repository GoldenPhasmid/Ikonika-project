import QtQuick
import QtQuick.Layouts
import "MobileIcons.js" as Icons

MobileTextField
{
    id: root

    Layout.fillWidth: true
    Layout.preferredHeight: 43

    placeholderText: qsTr("Search words...")

    leftPadding: 14 + icon.width + AppMetrics.gapSmall
    rightPadding: 14

    font.pixelSize: AppMetrics.fontBody

    background: Rectangle
    {
        radius: AppMetrics.radiusMedium
        color: AppTheme.colourPanel

        border.width: AppMetrics.borderWidth
        border.color: root.activeFocus ? AppTheme.colourAccent : AppTheme.colourBorder

        MobileVectorIcon
        {
            id: icon

            anchors.left: parent.left
            anchors.leftMargin: 14
            anchors.verticalCenter: parent.verticalCenter

            width: AppMetrics.fontSmall
            height: AppMetrics.fontSmall

            svgPath: Icons.Icons.search.path
            color: AppTheme.colourMuted
        }
    }
}
