import QtQuick
import QtQuick.Layouts

ColumnLayout
{
    id: root

    required property var store
    required property var session
    property var triage: null
    property bool wordRangeInline: false

    signal placementRequested()

    readonly property var exerciseTypes: [
        { key: "audio", label: qsTr("Write what you hear"), icon: "speaker_on", on: root.session.audioExercise },
        { key: "word", label: qsTr("Write the word"), icon: "edit", on: root.session.typeWord },
        { key: "gap", label: qsTr("Fill the gap"), icon: "edit", on: root.session.gapExercise }
    ]

    readonly property int exerciseTypesOn: root.exerciseTypes.filter(Type => Type.on).length

    function toggleExerciseType(type)
    {
        if (type.on && root.exerciseTypesOn <= 1)
        {
            return
        }

        if (type.key === "audio")
        {
            root.session.audioExercise = !type.on
        }
        else if (type.key === "word")
        {
            root.session.typeWord = !type.on
        }
        else
        {
            root.session.gapExercise = !type.on
        }
    }

    Layout.fillWidth: true
    spacing: AppMetrics.gapLarge

    DesktopSettingsGroup
    {
        visible: !root.session.review
        title: qsTr("Daily")

        DesktopSettingsSlider
        {
            title: qsTr("Words per training")
            value: root.store.trainingWordCount
            from: 10
            to: 25
            step: 5

            onEdited: function(newValue) { root.store.trainingWordCount = newValue }
        }

        DesktopSettingsSlider
        {
            title: qsTr("Trainings per day")
            description: qsTr("Daily goal: %1 words, %2 trainings of %3")
                .arg(root.store.dailyGoal).arg(root.store.sessionsPerDay).arg(root.store.trainingWordCount)
            value: root.store.sessionsPerDay
            from: 0
            to: root.store.maximumSessionsPerDay
            lastRow: true

            onEdited: function(newValue) { root.store.sessionsPerDay = newValue }
        }
    }

    DesktopSettingsGroup
    {
        visible: root.session.review
        title: qsTr("Daily")

        DesktopSettingsSlider
        {
            title: qsTr("Words per review")
            value: root.store.reviewWordCount
            from: 10
            to: 25
            step: 5

            onEdited: function(newValue) { root.store.reviewWordCount = newValue }
        }

        DesktopSettingsRow
        {
            text: qsTr("Missed review costs a box")
            description: qsTr("Off keeps the word in its box until it is reviewed")

            DesktopToggle
            {
                accessibleName: qsTr("Missed review costs a box")
                on: root.store.missedReviewDemotes

                onClicked: root.store.missedReviewDemotes = !root.store.missedReviewDemotes
            }
        }

        DesktopSettingsSlider
        {
            title: qsTr("Drop learning status after")
            description: qsTr("A word left in the first box this long without a review goes back to the pool")
            value: root.store.daysInFirstStage
            from: 1
            to: 10
            suffix: qsTr("days")
            lastRow: true

            onEdited: function(newValue) { root.store.daysInFirstStage = newValue }
        }
    }

    DesktopSettingsGroup
    {
        visible: !root.session.review && root.wordRangeInline && root.triage !== null
        title: qsTr("New words")

        DesktopSettingsRow
        {
            text: qsTr("Words from")
            description: qsTr("Training offers new words after this rank")
            lastRow: true

            DesktopDropdownButton
            {
                id: rangeButton

                objectName: "settings_word_range_button"

                Layout.fillWidth: false
                Layout.preferredWidth: AppMetrics.settingsControlWidth
                popover: rangePopover
                text: root.triage && root.triage.startRank > 0
                    ? qsTr("#%1").arg(Qt.locale().toString(root.triage.startRank, "f", 0)) : qsTr("The top")

                DesktopPopover
                {
                    id: rangePopover

                    y: rangeButton.height + AppMetrics.gapTight
                    width: Math.max(rangeButton.width, AppMetrics.popoverWidth)
                    padding: AppMetrics.gapMedium
                    topPadding: AppMetrics.gapMedium

                    DesktopWordRangePanel
                    {
                        Layout.preferredWidth: rangePopover.availableWidth
                        Layout.preferredHeight: AppMetrics.popoverListHeight
                        triage: root.triage

                        onPlacementRequested:
                        {
                            rangePopover.close()
                            root.placementRequested()
                        }
                    }
                }
            }
        }
    }

    DesktopSettingsGroup
    {
        visible: !root.session.review
        title: qsTr("Flip cards")

        DesktopSettingsSlider
        {
            title: qsTr("Times known per word")
            value: root.session.flipCount
            from: 1
            to: root.session.maximumFlipCount
            valueText: qsTr("%1×").arg(root.session.flipCount)
            lastRow: true

            onEdited: function(newValue) { root.session.flipCount = newValue }
        }
    }

    DesktopSettingsGroup
    {
        title: qsTr("Multiple choice")

        DesktopSettingsRow
        {
            text: qsTr("Multiple choice stage")

            DesktopToggle
            {
                accessibleName: qsTr("Multiple choice stage")
                on: root.session.choiceStage

                onClicked: root.session.choiceStage = !root.session.choiceStage
            }
        }

        DesktopSettingsSlider
        {
            title: qsTr("Rounds per word")
            value: root.session.choiceCount
            from: 1
            to: root.session.maximumStageRepeatCount
            valueText: qsTr("%1×").arg(root.session.choiceCount)
            interactive: root.session.choiceStage

            onEdited: function(newValue) { root.session.choiceCount = newValue }
        }

        DesktopSettingsRow
        {
            text: qsTr("Ask with")

            DesktopSelect
            {
                Layout.fillWidth: false
                Layout.preferredWidth: AppMetrics.settingsControlWidth
                heading: qsTr("Ask with")
                options: root.store.questionSideNames
                currentIndex: root.session.choiceSide
                enabled: root.session.choiceStage && !root.session.choiceVoice

                onChosen: function(index) { root.session.choiceSide = index }
            }
        }

        DesktopSettingsRow
        {
            text: qsTr("Ask by voice")
            description: qsTr("The word is played, not shown")
            lastRow: true

            DesktopToggle
            {
                accessibleName: qsTr("Ask by voice")
                on: root.session.choiceVoice
                enabled: root.session.choiceStage && root.store.audioAvailable

                onClicked: root.session.choiceVoice = !root.session.choiceVoice
            }
        }
    }

    DesktopSettingsGroup
    {
        title: qsTr("Typing")

        DesktopSettingsRow
        {
            text: qsTr("Exercises")
            description: root.exerciseTypesOn <= 1 ? qsTr("At least one stays on") : ""
            stacked: true

            RowLayout
            {
                Layout.fillWidth: true
                spacing: AppMetrics.gapSmall

                Repeater
                {
                    model: root.exerciseTypes

                    delegate: DesktopToggleChip
                    {
                        required property var modelData

                        Layout.preferredWidth: 1
                        text: modelData.label
                        iconName: modelData.icon
                        on: modelData.on

                        onClicked: root.toggleExerciseType(modelData)
                    }
                }
            }
        }

        DesktopSettingsSlider
        {
            title: qsTr("Correct answers to master a word")
            value: root.session.repeatsToMaster
            from: 1
            to: 10

            onEdited: function(newValue) { root.session.repeatsToMaster = newValue }
        }

        DesktopSettingsRow
        {
            text: qsTr("Require exact accents")
            lastRow: true

            DesktopToggle
            {
                accessibleName: qsTr("Require exact accents")
                on: root.session.requireExactAccents

                onClicked: root.session.requireExactAccents = !root.session.requireExactAccents
            }
        }
    }

    DesktopSettingsGroup
    {
        title: qsTr("Sentences")

        DesktopSettingsRow
        {
            text: qsTr("Sentence stage")
            description: qsTr("Build sentences that use the session's words")

            DesktopToggle
            {
                accessibleName: qsTr("Sentence stage")
                on: root.session.sentenceStage

                onClicked: root.session.sentenceStage = !root.session.sentenceStage
            }
        }

        DesktopSettingsRow
        {
            text: qsTr("Hard mode")
            description: qsTr("No word tiles, the sentence is typed in full")
            lastRow: true

            DesktopToggle
            {
                accessibleName: qsTr("Hard mode")
                on: root.session.sentenceTyping
                enabled: root.session.sentenceStage

                onClicked: root.session.sentenceTyping = !root.session.sentenceTyping
            }
        }
    }

    DesktopSettingsGroup
    {
        title: qsTr("Speed match")

        DesktopSettingsRow
        {
            text: qsTr("Speed match stage")

            DesktopToggle
            {
                accessibleName: qsTr("Speed match stage")
                on: root.session.matchStage

                onClicked: root.session.matchStage = !root.session.matchStage
            }
        }

        DesktopSettingsSlider
        {
            title: qsTr("Rounds per word")
            value: root.session.matchCount
            from: 1
            to: root.session.maximumStageRepeatCount
            valueText: qsTr("%1×").arg(root.session.matchCount)
            interactive: root.session.matchStage

            onEdited: function(newValue) { root.session.matchCount = newValue }
        }

        DesktopSettingsRow
        {
            text: qsTr("Match by voice")
            description: qsTr("Words are played, not shown")
            lastRow: true

            DesktopToggle
            {
                accessibleName: qsTr("Match by voice")
                on: root.session.matchVoice
                enabled: root.session.matchStage && root.store.audioAvailable

                onClicked: root.session.matchVoice = !root.session.matchVoice
            }
        }
    }
}
