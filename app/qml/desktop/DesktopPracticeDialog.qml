import QtQuick
import QtQuick.Layouts

DesktopStartDialog
{
    id: root

    property var vocabulary: null

    readonly property PracticeSelection selection: PracticeSelection
    {
        vocabulary: root.vocabulary
    }

    signal exerciseChosen(string exercise, int cardCount, int repetitionCount, var filterKeys, bool reverse)

    readonly property var exercises: [
        { key: "flashcards", iconName: "exercise_flashcards", label: qsTr("Flashcards") },
        { key: "flip", iconName: "exercise_flip", label: qsTr("Flip") },
        { key: "choice", iconName: "exercise_choice", label: qsTr("Multiple choice") },
        { key: "correctincorrect", iconName: "check", label: qsTr("Do you remember?") },
        { key: "written", iconName: "exercise_written", label: qsTr("Write the translation") },
        { key: "gap", iconName: "edit", label: qsTr("Fill the gap") },
        { key: "audio", iconName: "speaker_on", label: qsTr("Write what you hear") },
        { key: "speedmatch", iconName: "exercise_speedmatch", label: qsTr("Match pairs") }
    ]

    readonly property var directions: [
        { reverse: false, label: qsTr("Word → translation") },
        { reverse: true, label: qsTr("Translation → word") }
    ]

    function openFor()
    {
        if (root.chosenKey.length === 0)
        {
            root.chosenKey = root.exercises[0].key
        }

        root.selection.reset()
        root.open()
    }

    title: qsTr("Practice")
    choices: root.exercises
    canStart: root.selection.wordCount > 0
    startLabel: qsTr("Start · %n cards", "", root.selection.cardCount)

    onChoiceActivated: function(key) { root.chosenKey = key }

    onStartRequested:
    {
        const exercise = root.chosenKey
        const cardCount = root.selection.cardCount
        const repetitionCount = root.selection.repetitionCount
        const filterKeys = root.selection.filterKeys
        const reverse = exercise === "written" && root.selection.reverse

        root.closeThen(function() { root.exerciseChosen(exercise, cardCount, repetitionCount, filterKeys, reverse) })
    }

    settings: [
        ColumnLayout
        {
            Layout.fillWidth: true
            Layout.topMargin: AppMetrics.gapMedium
            spacing: AppMetrics.gapSmall

            DesktopLabel
            {
                Layout.fillWidth: true
                text: qsTr("Words")
                font.weight: Font.Medium
            }

            RowLayout
            {
                Layout.fillWidth: true
                spacing: AppMetrics.gapSmall

                Repeater
                {
                    model: root.selection.filters

                    delegate: DesktopChip
                    {
                        required property var modelData

                        objectName: "practice_filter_" + modelData.key

                        Layout.fillWidth: true
                        Layout.preferredWidth: 1

                        text: modelData.label
                        count: modelData.count.toString()
                        enabled: root.selection.isAvailable(modelData)
                        selected: root.selection.isSelected(modelData.key)

                        onClicked: root.selection.toggleFilter(modelData.key)
                    }
                }
            }
        },
        ColumnLayout
        {
            Layout.fillWidth: true
            Layout.topMargin: AppMetrics.gapMedium
            visible: root.chosenKey === "written"
            spacing: AppMetrics.gapSmall

            DesktopLabel
            {
                Layout.fillWidth: true
                text: qsTr("Direction")
                font.weight: Font.Medium
            }

            RowLayout
            {
                Layout.fillWidth: true
                spacing: AppMetrics.gapSmall

                Repeater
                {
                    model: root.directions

                    delegate: DesktopChip
                    {
                        required property var modelData

                        objectName: modelData.reverse ? "practice_direction_reverse" : "practice_direction_forward"

                        Layout.fillWidth: true
                        Layout.preferredWidth: 1

                        text: modelData.label
                        selected: root.selection.reverse === modelData.reverse

                        onClicked: root.selection.reverse = modelData.reverse
                    }
                }
            }
        },
        RowLayout
        {
            Layout.fillWidth: true
            spacing: AppMetrics.gapLarge

            DesktopSettingsSlider
            {
                Layout.preferredWidth: 1
                title: qsTr("Cards")
                inset: 0
                lastRow: true
                interactive: root.selection.canChooseCards
                value: root.selection.cardCount
                from: root.selection.minimumCards
                to: root.selection.maximumCards
                step: 1

                onEdited: function(newValue) { root.selection.cardCount = newValue }
            }

            DesktopSettingsSlider
            {
                Layout.preferredWidth: 1
                title: qsTr("Repetitions")
                inset: 0
                lastRow: true
                value: root.selection.repetitionCount
                from: root.selection.minimumRepetitions
                to: root.selection.maximumRepetitions
                step: root.selection.repetitionStep

                onEdited: function(newValue) { root.selection.repetitionCount = newValue }
            }
        }
    ]
}
