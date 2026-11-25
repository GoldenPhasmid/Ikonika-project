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
    property bool narrow: false

    readonly property var translations: root.meaning.translations || []

    spacing: AppMetrics.gapTight

    GridLayout
    {
        Layout.fillWidth: true
        columnSpacing: AppMetrics.gapSmall
        rowSpacing: AppMetrics.gapTight

        DesktopLabel
        {
            Layout.row: 0
            Layout.column: 0
            visible: root.number > 0 && !root.narrow
            text: root.number
            font.weight: Font.Bold
            color: AppTheme.colourMuted
        }

        DesktopGistBadge
        {
            Layout.row: 0
            Layout.column: root.narrow ? 0 : 1
            label: root.meaning.gist || ""
        }

        Item
        {
            Layout.row: 0
            Layout.column: 2
            Layout.fillWidth: true
            visible: root.narrow
        }

        Flow
        {
            Layout.row: root.narrow ? 1 : 0
            Layout.column: root.narrow ? 0 : 2
            Layout.columnSpan: root.narrow ? 5 : 1
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
            Layout.row: 0
            Layout.column: 3
            visible: root.learned
            stage: root.meaning.learnedStage
            stageCount: root.stageCount
            completed: !!root.meaning.completed
        }

        DesktopIcon
        {
            Layout.row: 0
            Layout.column: 4
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
