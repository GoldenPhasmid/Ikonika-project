import QtQuick
import QtQuick.Layouts
import "MobileIcons.js" as Icons

MobileCard
{
    id: root

    required property var meaning
    property int number: 1
    property var translations: []
    property int stageCount: 1
    property bool collapsible: false
    property bool expanded: !root.collapsible

    signal addRequested()
    signal editRequested()
    signal translationPicked(var translation)
    signal wordTapped(string word, string sentence)

    readonly property bool learned: root.meaning.learnedStage >= 0
    readonly property bool tinted: root.learned
    readonly property color stateColour: root.meaning.completed ? AppTheme.colourCorrect : AppTheme.colourAccent
    readonly property var cardTranslations: (root.meaning.cardTranslation || "").split(",")
        .map(Translation => Translation.trim().toLowerCase())
        .filter(Translation => Translation.length > 0)

    spacing: AppMetrics.gapMedium
    clip: true
    color: root.tinted && !root.expanded ? Qt.tint(AppTheme.colourPanel, AppTheme.faded(root.stateColour, AppTheme.alphaSoft))
                                         : AppTheme.colourPanel
    border.color: root.tinted ? AppTheme.faded(root.stateColour, AppTheme.alphaHalf) : AppTheme.colourBorder

    Behavior on implicitHeight { NumberAnimation { duration: AppMetrics.motionBase; easing.type: AppMetrics.motionEasing } }

    TapHandler
    {
        enabled: !root.expanded

        onTapped: root.expanded = true
    }

    MobileMeaningSummary
    {
        Layout.fillWidth: true
        visible: !root.expanded

        meaning: root.meaning
        number: root.number
        picked: root.cardTranslations
        pickedColour: root.stateColour
        stageCount: root.stageCount
    }

    RowLayout
    {
        Layout.fillWidth: true
        visible: root.expanded
        spacing: AppMetrics.gapSmall

        Text
        {
            text: root.number
            font.pixelSize: AppMetrics.fontBody
            font.weight: Font.Bold
            font.family: AppMetrics.fontFamily
            color: AppTheme.colourMuted
        }

        MobileGistTag
        {
            text: root.meaning.gist
        }

        Item { Layout.fillWidth: true }

        MobileProgressRing
        {
            visible: root.learned
            stage: root.meaning.learnedStage
            stageCount: root.stageCount
            completed: !!root.meaning.completed
        }

        MobileActionIconButton
        {
            iconSvg: root.learned ? Icons.Icons.edit.path : Icons.Icons.plus.path
            onClicked: root.learned ? root.editRequested() : root.addRequested()
        }

        MobileActionIconButton
        {
            visible: root.collapsible
            iconSvg: Icons.Icons.chevron_up.path
            iconWidth: Icons.Icons.chevron_up.width
            iconHeight: Icons.Icons.chevron_up.height

            onClicked: root.expanded = false
        }
    }

    MobileMeaningDetails
    {
        Layout.fillWidth: true
        visible: root.expanded
        meaning: root.meaning

        onWordTapped: function(word, sentence) { root.wordTapped(word, sentence) }
    }

    ColumnLayout
    {
        Layout.fillWidth: true
        spacing: AppMetrics.gapSmall
        visible: root.expanded && root.translations.length > 0

        Repeater
        {
            model: root.translations

            delegate: MobileTranslationChip
            {
                required property var modelData

                text: modelData.text
                relative: modelData.relative
                hasProbability: modelData.hasProbability
                learnedStage: modelData.learnedStage

                onPicked: root.translationPicked(modelData)
            }
        }
    }
}
