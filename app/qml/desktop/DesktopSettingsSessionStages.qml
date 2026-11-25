import QtQuick
import QtQuick.Layouts

ColumnLayout
{
    id: root

    required property var store
    required property var session

    readonly property var exerciseTypes: [
        { key: "audio", label: qsTr("Write what you hear"), icon: "speaker_on", on: root.session.audioExercise },
        { key: "word", label: qsTr("Write the word"), icon: "edit", on: root.session.typeWord },
        { key: "gap", label: qsTr("Fill the gap"), icon: "edit", on: root.session.gapExercise }
    ]

    readonly property int exerciseTypesOn: root.exerciseTypes.filter(Type => Type.on).length

    readonly property var lockedStage: root.session.review
        ? { key: "review", label: qsTr("Review card"), icon: "session_review" }
        : { key: "flashcards", label: qsTr("Flashcards"), icon: "exercise_flashcards" }

    readonly property var stages: [
        ...(root.session.review ? [] : [{ key: "flip", label: qsTr("Flip cards"), icon: "exercise_flip" }]),
        { key: "choice", label: qsTr("Multiple choice"), icon: "exercise_choice" },
        { key: "typing", label: qsTr("Typing"), icon: "exercise_written" },
        { key: "sentences", label: qsTr("Sentences"), icon: "session_sentences" },
        { key: "match", label: qsTr("Speed match"), icon: "exercise_speedmatch" }
    ]

    property string selectedStage: root.session.review ? "choice" : "flip"

    readonly property Item selectedChip:
    {
        const Index = root.stages.findIndex(Stage => Stage.key === root.selectedStage)

        return Index >= 0 && stageRepeater.count > Index ? stageRepeater.itemAt(Index) : null
    }

    function isStageOn(key)
    {
        if (key === "choice")
        {
            return root.session.choiceStage
        }

        if (key === "sentences")
        {
            return root.session.sentenceStage
        }

        if (key === "match")
        {
            return root.session.matchStage
        }

        return true
    }

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
            lastRow: true

            onEdited: function(newValue) { root.store.reviewWordCount = newValue }
        }
    }

    ColumnLayout
    {
        id: stageColumn

        Layout.fillWidth: true
        spacing: AppMetrics.gapSmall

        DesktopSectionLabel
        {
            text: qsTr("Stages")
        }

        RowLayout
        {
            id: stageRow

            Layout.fillWidth: true
            spacing: AppMetrics.gapSmall

            DesktopToggleChip
            {
                objectName: "stage_" + root.lockedStage.key
                Layout.fillWidth: false
                text: root.lockedStage.label
                iconName: root.lockedStage.icon
                on: true
                locked: true
            }

            Rectangle
            {
                Layout.fillHeight: true
                Layout.preferredWidth: AppMetrics.borderWidth
                Layout.topMargin: AppMetrics.gapSmall
                Layout.bottomMargin: AppMetrics.gapSmall
                Layout.leftMargin: AppMetrics.gapTight
                Layout.rightMargin: AppMetrics.gapTight
                color: AppTheme.colourBorder
            }

            Repeater
            {
                id: stageRepeater

                model: root.stages

                delegate: DesktopToggleChip
                {
                    required property var modelData

                    objectName: "stage_" + modelData.key
                    Layout.preferredWidth: 1
                    text: modelData.label
                    iconName: modelData.icon
                    on: root.isStageOn(modelData.key)
                    selected: root.selectedStage === modelData.key

                    onClicked: root.selectedStage = modelData.key
                }
            }
        }

        Item
        {
            id: dropdown

            Layout.fillWidth: true
            implicitHeight: stagePanel.implicitHeight

            Item
            {
                id: tabJoint

                readonly property real stroke: AppMetrics.dashStrokeWidth

                z: 1
                visible: root.selectedChip !== null
                x: root.selectedChip ? stageRow.x + root.selectedChip.x : 0
                y: -stageColumn.spacing - tabJoint.stroke - AppMetrics.gapTight
                width: root.selectedChip ? root.selectedChip.width : 0
                height: stageColumn.spacing + tabJoint.stroke * 2 + AppMetrics.gapTight + AppMetrics.borderWidth

                Rectangle
                {
                    anchors.fill: parent
                    anchors.leftMargin: tabJoint.stroke
                    anchors.rightMargin: tabJoint.stroke
                    color: AppTheme.colourPanel
                }

                Rectangle
                {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.topMargin: AppMetrics.gapTight
                    anchors.bottomMargin: AppMetrics.borderWidth
                    width: tabJoint.stroke
                    color: AppTheme.colourAccent
                }

                Rectangle
                {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.topMargin: AppMetrics.gapTight
                    anchors.bottomMargin: AppMetrics.borderWidth
                    width: tabJoint.stroke
                    color: AppTheme.colourAccent
                }
            }

            DesktopSettingsGroup
            {
                id: stagePanel

                width: dropdown.width
                outlineColour: AppTheme.colourAccent
                outlineWidth: AppMetrics.dashStrokeWidth
                squareTopRight: root.selectedChip !== null
                    && stageRow.x + root.selectedChip.x + root.selectedChip.width >= dropdown.width - AppMetrics.borderWidth

                DesktopSettingsSlider
                {
                    visible: root.selectedStage === "flip"
                    title: qsTr("Times known per word")
                    value: root.session.flipCount
                    from: 1
                    to: root.session.maximumFlipCount
                    valueText: qsTr("%1×").arg(root.session.flipCount)
                    lastRow: true

                    onEdited: function(newValue) { root.session.flipCount = newValue }
                }

                ColumnLayout
                {
                    visible: root.selectedStage === "choice"
                    Layout.fillWidth: true
                    spacing: 0

                    DesktopSettingsRow
                    {
                        text: qsTr("Enabled")

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

                ColumnLayout
                {
                    visible: root.selectedStage === "typing"
                    Layout.fillWidth: true
                    spacing: 0

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

                ColumnLayout
                {
                    visible: root.selectedStage === "sentences"
                    Layout.fillWidth: true
                    spacing: 0

                    DesktopSettingsRow
                    {
                        text: qsTr("Enabled")
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

                ColumnLayout
                {
                    visible: root.selectedStage === "match"
                    Layout.fillWidth: true
                    spacing: 0

                    DesktopSettingsRow
                    {
                        text: qsTr("Enabled")

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
        }
    }

    DesktopSettingsGroup
    {
        visible: root.session.review
        title: qsTr("Spaced Repetition Scheduling")

        DesktopSettingsRow
        {
            text: qsTr("Learning Intensity")
            description: qsTr("Higher intensity: more reviews and shorter breaks in between.")

            DesktopSegmentedPill
            {
                objectName: "learning_intensity"

                accessibleName: qsTr("Learning Intensity")
                options: [qsTr("Light"), qsTr("Normal"), qsTr("Hard"), qsTr("Custom")]
                currentIndex: root.store.learningIntensity

                onChosen: function(index) { root.store.learningIntensity = index }
            }
        }

        DesktopSettingsSlider
        {
            visible: root.store.learningIntensity === 3
            title: qsTr("Recall target")
            value: root.store.customTargetRetention
            from: root.store.minimumTargetRetention
            to: root.store.maximumTargetRetention
            valueText: qsTr("%1%").arg(value)

            onEdited: function(newValue) { root.store.customTargetRetention = newValue }
        }

        DesktopSettingsSlider
        {
            visible: root.store.learningIntensity === 3
            title: qsTr("Learned at stability")
            value: root.store.customLearnedStability
            from: root.store.minimumLearnedStability
            to: root.store.maximumLearnedStability
            step: 10

            onEdited: function(newValue) { root.store.customLearnedStability = newValue }
        }

        DesktopSettingsSlider
        {
            visible: root.store.learningIntensity === 3
            title: qsTr("Quick review at stability")
            value: root.store.customQuickReviewStability
            from: 1
            to: root.store.maximumQuickReviewStability

            onEdited: function(newValue) { root.store.customQuickReviewStability = newValue }
        }

        ScheduleTimeline
        {
            Layout.fillWidth: true
            Layout.leftMargin: AppMetrics.gapLarge
            Layout.rightMargin: AppMetrics.gapLarge
            Layout.topMargin: AppMetrics.gapWide
            Layout.bottomMargin: AppMetrics.gapLarge
            paths: root.store.schedulePreview
        }
    }
}
