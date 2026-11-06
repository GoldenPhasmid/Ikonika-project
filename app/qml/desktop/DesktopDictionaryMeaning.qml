import QtQuick
import QtQuick.Layouts

DesktopCard
{
    id: root

    required property var meaning
    property int number: 1
    property var translations: []
    property int columns: 3
    property int stageCount: 1
    property bool collapsible: false
    property bool expanded: !root.collapsible

    signal addRequested()
    signal editRequested()
    signal translationPicked(var translation)
    signal wordClicked(string word, string sentence)

    readonly property bool learned: root.meaning.learnedStage >= 0
    readonly property bool hovered: !root.expanded && hoverHandler.hovered
    readonly property bool tinted: root.learned
    readonly property color stateColour: root.meaning.completed ? AppTheme.colourCorrect : AppTheme.colourAccent
    readonly property var cardTranslations: (root.meaning.cardTranslation || "").split(",")
        .map(Translation => Translation.trim().toLowerCase())
        .filter(Translation => Translation.length > 0)

    Layout.fillWidth: true
    padding: AppMetrics.gapLarge
    clip: true
    spacing: AppMetrics.gapCompact
    color:
    {
        if (root.hovered)
        {
            return AppTheme.colourSunken
        }

        return root.tinted && !root.expanded ? Qt.tint(AppTheme.colourPanel, AppTheme.faded(root.stateColour, AppTheme.alphaSoft))
                                             : AppTheme.colourPanel
    }
    border.color:
    {
        if (root.hovered)
        {
            return AppTheme.colourOutline
        }

        return root.tinted ? AppTheme.faded(root.stateColour, AppTheme.alphaHalf) : AppTheme.colourBorder
    }

    Behavior on implicitHeight { NumberAnimation { duration: AppMetrics.motionBase; easing.type: AppMetrics.motionEasing } }

    Accessible.role: root.expanded ? Accessible.Grouping : Accessible.Button
    Accessible.name: root.meaning.gist
    Accessible.onPressAction: root.expanded = true

    HoverHandler
    {
        id: hoverHandler

        enabled: !root.expanded
        margin: root.padding
        cursorShape: Qt.PointingHandCursor
    }

    TapHandler
    {
        enabled: !root.expanded
        margin: root.padding

        onTapped: root.expanded = true
    }

    DesktopMeaningSummary
    {
        Layout.fillWidth: true
        visible: !root.expanded

        meaning: root.meaning
        number: root.number
        learned: root.learned
        stageCount: root.stageCount
        picked: root.cardTranslations
        pickedColour: root.stateColour
    }

    RowLayout
    {
        Layout.fillWidth: true
        visible: root.expanded
        spacing: AppMetrics.gapSmall

        DesktopLabel
        {
            text: root.number
            font.pixelSize: AppMetrics.dictionaryFontBody
            font.weight: Font.Bold
            color: AppTheme.colourMuted
        }

        DesktopGistBadge
        {
            label: root.meaning.gist
        }

        Item { Layout.fillWidth: true }

        DesktopProgressRing
        {
            visible: root.learned
            stage: root.meaning.learnedStage
            stageCount: root.stageCount
            completed: !!root.meaning.completed
        }

        DesktopButton
        {
            iconName: root.learned ? "edit" : "plus"
            text: root.learned ? qsTr("Edit") : qsTr("Add")

            onClicked: root.learned ? root.editRequested() : root.addRequested()
        }

        DesktopIconButton
        {
            visible: root.collapsible
            iconName: "chevron_up"
            iconSize: AppMetrics.iconSizeSmall
            text: qsTr("Show less")

            onClicked: root.expanded = false
        }
    }

    DesktopMeaningDetails
    {
        Layout.fillWidth: true
        visible: root.expanded
        meaning: root.meaning

        onWordClicked: function(word, sentence) { root.wordClicked(word, sentence) }
    }

    GridLayout
    {
        Layout.fillWidth: true
        Layout.topMargin: AppMetrics.gapTight
        visible: root.expanded && root.translations.length > 0
        columns: root.columns
        columnSpacing: AppMetrics.gapSmall
        rowSpacing: AppMetrics.gapSmall

        Repeater
        {
            model: root.translations

            delegate: DesktopTranslationChip
            {
                required property var modelData

                Layout.preferredWidth: 1
                text: modelData.text
                relative: modelData.relative
                hasProbability: modelData.hasProbability
                learnedStage: modelData.learnedStage

                onClicked: root.translationPicked(modelData)
            }
        }
    }
}
