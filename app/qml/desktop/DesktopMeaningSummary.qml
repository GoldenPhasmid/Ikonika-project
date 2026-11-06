import QtQuick
import QtQuick.Layouts

ColumnLayout
{
    id: root

    property var meaning: ({})
    property var picked: []
    property int number: 0
    property bool learned: false
    property int stageCount: 1
    property color pickedColour: AppTheme.colourAccent

    readonly property var translations: root.meaning.translations || []

    spacing: AppMetrics.gapTight

    RowLayout
    {
        Layout.fillWidth: true
        spacing: AppMetrics.gapSmall

        DesktopLabel
        {
            visible: root.number > 0
            text: root.number
            font.weight: Font.Bold
            color: AppTheme.colourMuted
        }

        DesktopGistBadge
        {
            label: root.meaning.gist || ""
        }

        Flow
        {
            Layout.fillWidth: true
            Layout.preferredWidth: 0

            Repeater
            {
                model: root.translations

                delegate: Row
                {
                    id: translationItem

                    required property string modelData
                    required property int index

                    readonly property bool chosen: root.picked.indexOf(translationItem.modelData.trim().toLowerCase()) >= 0

                    DesktopLabel
                    {
                        text: translationItem.modelData
                        font.weight: translationItem.chosen ? Font.DemiBold : Font.Normal
                        color: translationItem.chosen ? root.pickedColour : AppTheme.colourText
                    }

                    DesktopLabel
                    {
                        visible: translationItem.index < root.translations.length - 1
                        text: ", "
                        color: AppTheme.colourMuted
                    }
                }
            }
        }

        DesktopProgressRing
        {
            visible: root.learned
            stage: root.meaning.learnedStage
            stageCount: root.stageCount
            completed: !!root.meaning.completed
        }

        DesktopIcon
        {
            name: "chevron_down"
            width: AppMetrics.iconSizeSmall
            height: AppMetrics.iconSizeSmall
            color: AppTheme.colourMuted
        }
    }

    DesktopLabel
    {
        Layout.fillWidth: true
        visible: text.length > 0
        text: root.meaning.nativeDefinition || ""
        color: AppTheme.colourSubtle
        wrapMode: Text.WordWrap
    }
}
