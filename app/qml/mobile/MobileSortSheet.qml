import QtQuick
import QtQuick.Layouts

MobileBottomSheet
{
    id: root

    heading: qsTr("Sort")

    property var model: []
    property string currentKey: ""
    property bool descending: false

    signal chosen(string key, bool descending)

    readonly property var activeOption:
    {
        for (const option of root.model)
        {
            if (option.key === root.currentKey)
            {
                return option
            }
        }

        return null
    }

    RowLayout
    {
        Layout.fillWidth: true
        spacing: AppMetrics.gapSmall

        Rectangle
        {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            radius: AppMetrics.radiusSmall
            color: root.descending ? AppTheme.colourFaint : AppTheme.colourText

            Text
            {
                anchors.centerIn: parent
                text: root.activeOption ? root.activeOption.primary : ""
                font.pixelSize: AppMetrics.fontLabel
                font.weight: Font.Medium
                font.family: AppMetrics.fontFamily
                color: root.descending ? AppTheme.colourText : AppTheme.colourPanel
            }

            TapHandler { onTapped: root.chosen(root.currentKey, false) }
        }

        Rectangle
        {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            radius: AppMetrics.radiusSmall
            color: root.descending ? AppTheme.colourText : AppTheme.colourFaint

            Text
            {
                anchors.centerIn: parent
                text: root.activeOption ? root.activeOption.inverted : ""
                font.pixelSize: AppMetrics.fontLabel
                font.weight: Font.Medium
                font.family: AppMetrics.fontFamily
                color: root.descending ? AppTheme.colourPanel : AppTheme.colourText
            }

            TapHandler { onTapped: root.chosen(root.currentKey, true) }
        }
    }

    MobileOptionListPanel
    {
        Repeater
        {
            model: root.model

            delegate: MobileSettingsRow
            {
                id: optionRow

                required property var modelData
                required property int index

                title: optionRow.modelData.primary
                lastRow: optionRow.index === root.model.length - 1

                onClicked: root.chosen(optionRow.modelData.key, false)

                DesktopIcon
                {
                    visible: optionRow.modelData.key === root.currentKey
                    name: "check"
                    width: AppMetrics.iconSizeSmall
                    height: width
                    color: AppTheme.colourText
                }
            }
        }
    }
}
