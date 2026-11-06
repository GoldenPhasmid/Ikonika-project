import QtQuick
import QtQuick.Layouts

Item
{
    id: root

    property string title: ""
    property var options: []
    property bool lastRow: false

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
            Layout.fillWidth: true

            spacing: AppMetrics.gapSmall

            Repeater
            {
                model: root.options

                delegate: MobileFilterChip
                {
                    required property var modelData

                    label: modelData.label
                    count: modelData.count === undefined ? "" : modelData.count.toString()
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
