import QtQuick
import QtQuick.Layouts

MobileBottomSheet
{
    id: root

    property string exerciseKey: ""
    property string exerciseLabel: ""
    property var vocabulary: null

    readonly property int cardStep: 5

    readonly property PracticeSelection selection: PracticeSelection
    {
        vocabulary: root.vocabulary
        cardLimit: 50
    }

    readonly property var directionOptions: [
        { label: qsTr("Word → translation"), reverse: false, selected: !root.selection.reverse },
        { label: qsTr("Translation → word"), reverse: true, selected: root.selection.reverse }
    ]

    heading: root.exerciseLabel

    signal startRequested(bool reverse, int cardCount, int repetitionCount, var filterKeys)

    Component.onCompleted: root.selection.reset()

    MobileOptionListPanel
    {
        MobileFilterChipRow
        {
            title: qsTr("Words")
            fillWidth: true
            showCounts: false

            options: root.selection.filters.map(function(filter)
            {
                return {
                    key: filter.key,
                    label: filter.label,
                    count: filter.count,
                    selected: root.selection.isSelected(filter.key),
                    available: root.selection.isAvailable(filter)
                }
            })

            onChosen: function(option) { root.selection.toggleFilter(option.key) }
        }

        MobileFilterChipRow
        {
            visible: root.exerciseKey === "written"
            title: qsTr("Direction")

            options: root.directionOptions

            onChosen: function(option) { root.selection.reverse = option.reverse }
        }

        MobileSettingsSlider
        {
            title: qsTr("Cards")

            interactive: root.selection.maximumCards > root.cardStep
            value: root.selection.cardCount
            from: Math.min(root.cardStep, root.selection.maximumCards)
            to: root.selection.maximumCards
            step: root.cardStep

            onEdited: function(newValue) { root.selection.cardCount = newValue }
        }

        MobileSettingsSlider
        {
            title: qsTr("Repetitions")
            lastRow: true

            value: root.selection.repetitionCount
            from: root.selection.minimumRepetitions
            to: root.selection.maximumRepetitions
            step: root.selection.repetitionStep

            onEdited: function(newValue) { root.selection.repetitionCount = newValue }
        }
    }

    footer: MobileWideButton
    {
        label: qsTr("Start · %1 cards · %2 reps").arg(root.selection.cardCount).arg(root.selection.repetitionCount)
        interactive: root.selection.wordCount > 0

        onClicked: root.startRequested(root.exerciseKey === "written" && root.selection.reverse,
                                       root.selection.cardCount, root.selection.repetitionCount,
                                       root.selection.filterKeys)
    }
}
