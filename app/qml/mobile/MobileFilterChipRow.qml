import QtQuick
import QtQuick.Layouts

Item
{
    id: root

    property string title: ""
    property var options: []
    property bool lastRow: false
    property bool fillWidth: false
    property bool showCounts: true

    signal chosen(var option)

    Layout.fillWidth: true
    implicitHeight: content.implicitHeight + (AppMetrics.gapMedium * 2)

    ColumnLayout
    {
        id: content

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: AppMetrics.contentPadding
        anchors.rightMargin: AppMetrics.contentPadding
        anchors.verticalCenter: parent.verticalCenter

        spacing: AppMetrics.gapSmall

        Text
        {
            Layout.fillWidth: true

            text: root.title
            font.pixelSize: AppMetrics.fontLabel
            font.weight: Font.Medium
            font.family: AppMetrics.fontFamily
            color: AppTheme.colourText
        }

        Flow
        {
            id: chips

            Layout.fillWidth: true

            spacing: AppMetrics.gapSmall

            Repeater
            {
                model: root.options

                delegate: MobileFilterChip
                {
                    required property var modelData

                    width: root.fillWidth && root.options.length > 0
                        ? (chips.width - chips.spacing * (root.options.length - 1)) / root.options.length
                        : implicitWidth

                    label: modelData.label
                    count: !root.showCounts || modelData.count === undefined ? "" : modelData.count.toString()
                    selected: modelData.selected
                    enabled: modelData.available !== false
                    opacity: enabled ? 1.0 : AppTheme.opacityDisabled

                    onClicked: root.chosen(modelData)
                }
            }
        }
    }

    Rectangle
    {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.leftMargin: AppMetrics.contentPadding

        width: parent.width - AppMetrics.contentPadding
        height: AppMetrics.borderWidth

        visible: !root.lastRow
        color: AppTheme.colourBorder
    }
}
