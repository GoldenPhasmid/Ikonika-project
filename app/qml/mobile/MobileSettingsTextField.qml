import QtQuick
import QtQuick.Layouts

Item
{
    id: root

    required property string title

    property string text: ""
    property string placeholderText: ""
    property bool lastRow: false

    signal edited(string newText)

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
            elide: Text.ElideRight
        }

        Rectangle
        {
            Layout.fillWidth: true
            Layout.preferredHeight: 48

            radius: AppMetrics.radiusMedium
            color: "transparent"
            border.width: AppMetrics.borderWidth
            border.color: AppTheme.colourBorder

            MobileTextInput
            {
                id: field

                anchors.fill: parent
                anchors.leftMargin: AppMetrics.contentPadding
                anchors.rightMargin: AppMetrics.contentPadding
                verticalAlignment: TextInput.AlignVCenter

                text: root.text
                placeholderText: root.placeholderText

                font.pixelSize: AppMetrics.fontLabel
                font.family: AppMetrics.fontFamily
                color: AppTheme.colourText

                onEditingFinished: root.edited(field.text)
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
