import QtQuick
import QtQuick.Layouts
import "MobileIcons.js" as Icons

ColumnLayout
{
    id: root

    property var meaning: ({})
    property var picked: []
    property int number: 0
    property color pickedColour: AppTheme.colourAccent
    property int stageCount: 1

    readonly property var translations: root.meaning.translations || []
    readonly property var chosenTranslations: root.translations.filter(Text => root.picked.indexOf(Text.trim().toLowerCase()) >= 0)
    readonly property var shownTranslations: root.chosenTranslations
        .concat(root.translations.filter(Text => root.chosenTranslations.indexOf(Text) < 0))
        .slice(0, Math.max(root.summaryCount, root.chosenTranslations.length))
    readonly property int summaryCount: 3

    spacing: AppMetrics.gapTight

    RowLayout
    {
        Layout.fillWidth: true
        spacing: AppMetrics.gapSmall

        Text
        {
            visible: root.number > 0
            text: root.number
            font.pixelSize: AppMetrics.fontBody
            font.weight: Font.Bold
            font.family: AppMetrics.fontFamily
            color: AppTheme.colourMuted
        }

        MobileGistTag
        {
            text: root.meaning.gist || ""
        }

        Flow
        {
            Layout.fillWidth: true
            Layout.preferredWidth: 0

            Repeater
            {
                model: root.shownTranslations

                delegate: Row
                {
                    id: translationItem

                    required property string modelData
                    required property int index

                    readonly property bool chosen: root.picked.indexOf(translationItem.modelData.trim().toLowerCase()) >= 0

                    Text
                    {
                        text: translationItem.modelData
                        font.pixelSize: AppMetrics.fontBody
                        font.weight: translationItem.chosen ? Font.DemiBold : Font.Normal
                        font.family: AppMetrics.fontFamily
                        color: translationItem.chosen ? root.pickedColour : AppTheme.colourText
                    }

                    Text
                    {
                        visible: translationItem.index < root.shownTranslations.length - 1
                        text: ", "
                        font.pixelSize: AppMetrics.fontBody
                        font.family: AppMetrics.fontFamily
                        color: AppTheme.colourMuted
                    }
                }
            }
        }

        MobileProgressRing
        {
            visible: root.meaning.learnedStage >= 0
            stage: root.meaning.learnedStage
            stageCount: root.stageCount
            completed: !!root.meaning.completed
        }

        MobileVectorIcon
        {
            Layout.preferredWidth: AppMetrics.iconSizeSmall
            Layout.preferredHeight: AppMetrics.iconSizeSmall
            svgPath: Icons.Icons.chevron_down.path
            sourceWidth: Icons.Icons.chevron_down.width
            sourceHeight: Icons.Icons.chevron_down.height
            color: AppTheme.colourMuted
        }
    }

    Text
    {
        Layout.fillWidth: true
        visible: text.length > 0
        text: root.meaning.nativeDefinition || ""
        font.pixelSize: AppMetrics.fontSmall
        font.family: AppMetrics.fontFamily
        color: AppTheme.colourSubtle
        wrapMode: Text.WordWrap
    }
}
