import QtQuick
import QtQuick.Layouts

ColumnLayout
{
    id: root

    required property var store
    required property var session
    required property var optionPicker

    function pick(title, options, current, apply)
    {
        root.optionPicker.open(title, options, current, apply)
    }

    Layout.fillWidth: true
    spacing: AppMetrics.gapLarge

    MobileSettingsGroup
    {
        visible: !root.session.review
        title: qsTr("DAILY")

        MobileSettingsSlider
        {
            title: qsTr("Words per training")
            value: root.store.trainingWordCount
            from: 10
            to: 25
            step: 5

            onEdited: function(newValue) { root.store.trainingWordCount = newValue }
        }

        MobileSettingsSlider
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

    MobileSettingsGroup
    {
        visible: root.session.review
        title: qsTr("DAILY")

        MobileSettingsSlider
        {
            title: qsTr("Words per review")
            value: root.store.reviewWordCount
            from: 10
            to: 25
            step: 5

            onEdited: function(newValue) { root.store.reviewWordCount = newValue }
        }

        MobileSettingsRow
        {
            title: qsTr("Missed review costs a box")
            description: qsTr("Off keeps the word in its box until it is reviewed")

            MobileSettingsToggle
            {
                checked: root.store.missedReviewDemotes

                onToggled: root.store.missedReviewDemotes = !root.store.missedReviewDemotes
            }
        }

        MobileSettingsSlider
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

    MobileSettingsGroup
    {
        visible: !root.session.review
        title: qsTr("FLIP CARDS")

        MobileSettingsSlider
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

    MobileSettingsGroup
    {
        title: qsTr("MULTIPLE CHOICE")

        MobileSettingsRow
        {
            title: qsTr("Multiple choice stage")

            MobileSettingsToggle
            {
                checked: root.session.choiceStage

                onToggled: root.session.choiceStage = !root.session.choiceStage
            }
        }

        MobileSettingsSlider
        {
            title: qsTr("Rounds per word")
            value: root.session.choiceCount
            from: 1
            to: root.session.maximumStageRepeatCount
            valueText: qsTr("%1×").arg(root.session.choiceCount)
            interactive: root.session.choiceStage

            onEdited: function(newValue) { root.session.choiceCount = newValue }
        }

        MobileSettingsRow
        {
            title: qsTr("Ask with")

            MobileSettingsSelect
            {
                label: root.store.questionSideNames[root.session.choiceSide]
                interactive: root.session.choiceStage && !root.session.choiceVoice

                onClicked: root.pick(qsTr("Ask with"), root.store.questionSideNames, root.session.choiceSide,
                                     function(index) { root.session.choiceSide = index })
            }
        }

        MobileSettingsRow
        {
            title: qsTr("Ask by voice")
            description: qsTr("The word is played, not shown")
            lastRow: true

            MobileSettingsToggle
            {
                checked: root.session.choiceVoice
                interactive: root.session.choiceStage && root.store.audioAvailable

                onToggled: root.session.choiceVoice = !root.session.choiceVoice
            }
        }
    }

    MobileSettingsGroup
    {
        title: qsTr("TYPING")

        MobileExerciseTypesRow
        {
            session: root.session
        }

        MobileSettingsSlider
        {
            title: qsTr("Answers to master a word")
            value: root.session.repeatsToMaster
            from: 1
            to: 10

            onEdited: function(newValue) { root.session.repeatsToMaster = newValue }
        }

        MobileSettingsRow
        {
            title: qsTr("Require exact accents")
            description: qsTr("Marks missing or wrong accents as incorrect")
            lastRow: true

            MobileSettingsToggle
            {
                checked: root.session.requireExactAccents

                onToggled: root.session.requireExactAccents = !root.session.requireExactAccents
            }
        }
    }

    MobileSettingsGroup
    {
        title: qsTr("SENTENCES")

        MobileSettingsRow
        {
            title: qsTr("Sentence stage")
            description: qsTr("Build sentences that use the session's words")

            MobileSettingsToggle
            {
                checked: root.session.sentenceStage

                onToggled: root.session.sentenceStage = !root.session.sentenceStage
            }
        }

        MobileSettingsRow
        {
            title: qsTr("Hard mode")
            description: qsTr("No word tiles, the sentence is typed in full")
            lastRow: true

            MobileSettingsToggle
            {
                checked: root.session.sentenceTyping
                interactive: root.session.sentenceStage

                onToggled: root.session.sentenceTyping = !root.session.sentenceTyping
            }
        }
    }

    MobileSettingsGroup
    {
        title: qsTr("SPEED MATCH")

        MobileSettingsRow
        {
            title: qsTr("Speed match stage")

            MobileSettingsToggle
            {
                checked: root.session.matchStage

                onToggled: root.session.matchStage = !root.session.matchStage
            }
        }

        MobileSettingsSlider
        {
            title: qsTr("Rounds per word")
            value: root.session.matchCount
            from: 1
            to: root.session.maximumStageRepeatCount
            valueText: qsTr("%1×").arg(root.session.matchCount)
            interactive: root.session.matchStage

            onEdited: function(newValue) { root.session.matchCount = newValue }
        }

        MobileSettingsRow
        {
            title: qsTr("Match by voice")
            description: qsTr("Words are played, not shown")
            lastRow: true

            MobileSettingsToggle
            {
                checked: root.session.matchVoice
                interactive: root.session.matchStage && root.store.audioAvailable

                onToggled: root.session.matchVoice = !root.session.matchVoice
            }
        }
    }
}
