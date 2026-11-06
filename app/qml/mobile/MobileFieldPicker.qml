import QtQuick
import QtQuick.Layouts
import "MobileIcons.js" as Icons

ColumnLayout
{
    id: root

    property string label: ""
    property string value: ""
    property string description: ""

    signal picked()

    Layout.fillWidth: true
    spacing: AppMetrics.gapSmall

    MobileSectionLabel { text: root.label }

    Rectangle
    {
        Layout.fillWidth: true
        Layout.preferredHeight: 47

        radius: AppMetrics.radiusMedium
        color: "transparent"
        border.width: AppMetrics.borderWidth
        border.color: fieldPress.pressed ? AppTheme.colourAccent : AppTheme.colourBorder

        Text
        {
            anchors.left: parent.left
            anchors.right: fieldChevron.left
            anchors.leftMargin: AppMetrics.contentPadding
            anchors.rightMargin: AppMetrics.gapSmall
            anchors.verticalCenter: parent.verticalCenter

            text: root.value
            font.pixelSize: AppMetrics.fontLabel
            font.family: AppMetrics.fontFamily
            color: AppTheme.colourText
            elide: Text.ElideRight
        }

        MobileVectorIcon
        {
            id: fieldChevron

            anchors.right: parent.right
            anchors.rightMargin: AppMetrics.contentPadding
            anchors.verticalCenter: parent.verticalCenter

            width: 12
            height: 12

            sourceWidth: Icons.Icons.chevron_down.width
            sourceHeight: Icons.Icons.chevron_down.height
            svgPath: Icons.Icons.chevron_down.path
            color: AppTheme.colourText
        }

        MouseArea
        {
            id: fieldPress

            anchors.fill: parent
            onClicked: root.picked()
        }
    }

    Text
    {
        Layout.fillWidth: true

        visible: root.description.length > 0
        text: root.description
        font.pixelSize: AppMetrics.fontMeta
        font.family: AppMetrics.fontFamily
        color: AppTheme.colourSubtle
        wrapMode: Text.WordWrap
    }
}
