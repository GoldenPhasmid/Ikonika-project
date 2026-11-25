import QtQuick
import QtQuick.Layouts

ColumnLayout
{
    id: root

    required property var store
    required property var session
    required property var optionPicker

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

    readonly property var selectedEntry: root.stages.find(Stage => Stage.key === root.selectedStage) || root.stages[0]

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
            lastRow: true

            onEdited: function(newValue) { root.store.reviewWordCount = newValue }
        }
    }

    ColumnLayout
    {
        id: stageColumn

        Layout.fillWidth: true
        spacing: AppMetrics.gapSmall

        MobileSectionLabel
        {
            text: qsTr("STAGES")
        }

        RowLayout
        {
            id: stageRow

            Layout.fillWidth: true
            spacing: AppMetrics.gapTight

            MobileStageChip
            {
                objectName: "stage_" + root.lockedStage.key
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                label: root.lockedStage.label
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

                delegate: MobileStageChip
                {
                    required property var modelData

                    objectName: "stage_" + modelData.key
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    label: modelData.label
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

                readonly property real stroke: AppMetrics.borderWidth * 1.5

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

            MobileSettingsGroup
            {
                id: stagePanel

                width: dropdown.width
                title: ""
                outlineColour: AppTheme.colourAccent
                outlineWidth: AppMetrics.borderWidth * 1.5
                squareTopRight: root.selectedChip !== null
                    && stageRow.x + root.selectedChip.x + root.selectedChip.width >= dropdown.width - AppMetrics.borderWidth

                Text
                {
                    Layout.fillWidth: true
                    Layout.leftMargin: AppMetrics.contentPadding
                    Layout.rightMargin: AppMetrics.contentPadding
                    Layout.topMargin: AppMetrics.gapMedium
                    text: root.selectedEntry.label
                    font.pixelSize: AppMetrics.fontLabel
                    font.weight: Font.DemiBold
                    font.family: AppMetrics.fontFamily
                    color: AppTheme.colourText
                }

                MobileSettingsSlider
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

                    MobileSettingsRow
                    {
                        title: qsTr("Enabled")

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

                ColumnLayout
                {
                    visible: root.selectedStage === "typing"
                    Layout.fillWidth: true
                    spacing: 0

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

                ColumnLayout
                {
                    visible: root.selectedStage === "sentences"
                    Layout.fillWidth: true
                    spacing: 0

                    MobileSettingsRow
                    {
                        title: qsTr("Enabled")
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

                ColumnLayout
                {
                    visible: root.selectedStage === "match"
                    Layout.fillWidth: true
                    spacing: 0

                    MobileSettingsRow
                    {
                        title: qsTr("Enabled")

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
        }
    }

    MobileSettingsGroup
    {
        visible: root.session.review
        title: qsTr("SPACED REPETITION SCHEDULING")

        MobileSettingsWideRow
        {
            ColumnLayout
            {
                Layout.fillWidth: true
                spacing: AppMetrics.gapSmall

                Text
                {
                    Layout.fillWidth: true
                    text: qsTr("Learning Intensity")
                    font.pixelSize: AppMetrics.fontLabel
                    font.weight: Font.Medium
                    font.family: AppMetrics.fontFamily
                    color: AppTheme.colourText
                    elide: Text.ElideRight
                }

                Text
                {
                    Layout.fillWidth: true
                    text: qsTr("Higher intensity: more reviews and shorter breaks in between.")
                    font.pixelSize: AppMetrics.fontMeta
                    font.family: AppMetrics.fontFamily
                    color: AppTheme.colourSubtle
                    wrapMode: Text.WordWrap
                }

                MobileSegmentedPill
                {
                    objectName: "learning_intensity"

                    Layout.fillWidth: true
                    Layout.topMargin: AppMetrics.gapTight
                    options: [qsTr("Light"), qsTr("Normal"), qsTr("Hard"), qsTr("Custom")]
                    currentIndex: root.store.learningIntensity

                    onChosen: function(index) { root.store.learningIntensity = index }
                }
            }
        }

        MobileSettingsSlider
        {
            visible: root.store.learningIntensity === 3
            title: qsTr("Recall target")
            value: root.store.customTargetRetention
            from: root.store.minimumTargetRetention
            to: root.store.maximumTargetRetention
            valueText: qsTr("%1%").arg(value)

            onEdited: function(newValue) { root.store.customTargetRetention = newValue }
        }

        MobileSettingsSlider
        {
            visible: root.store.learningIntensity === 3
            title: qsTr("Learned at stability")
            value: root.store.customLearnedStability
            from: root.store.minimumLearnedStability
            to: root.store.maximumLearnedStability
            step: 10

            onEdited: function(newValue) { root.store.customLearnedStability = newValue }
        }

        MobileSettingsSlider
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
            Layout.leftMargin: AppMetrics.contentPadding
            Layout.rightMargin: AppMetrics.contentPadding
            Layout.topMargin: AppMetrics.gapWide
            Layout.bottomMargin: AppMetrics.gapLarge
            titleSize: AppMetrics.fontLabel
            paths: root.store.schedulePreview
        }
    }
}
