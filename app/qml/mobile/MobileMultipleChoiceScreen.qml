import QtQuick
import QtQuick.Layouts

Item
{
    id: root

    property var controller: MultipleChoiceController {}

    property var store: null

    property bool showComplete: true
    property bool inSession: false
    property bool cardEditable: false

    readonly property bool finished: root.controller.finished
    readonly property bool voiceHidden: root.controller.voicePrompt && !root.controller.answered

    signal done()
    signal endSessionRequested()
    signal editCardRequested()
    signal restartRequested()
    signal completeRequested()

    function stopAutoAdvance()
    {
        correctAdvanceDelay.stop()
    }

    function advance()
    {
        root.stopAutoAdvance()

        shell.swap(function() { root.controller.advance() })
    }

    Timer
    {
        id: correctAdvanceDelay

        interval: AppMetrics.exerciseAdvanceDelayCorrect
        onTriggered: root.advance()
    }

    Connections
    {
        target: root.controller

        function onAnswerGraded(correct)
        {
            optionGrid.armFeedback(correct)
        }

        function onStateChanged()
        {
            if (root.controller.finished || !root.controller.answered)
            {
                root.stopAutoAdvance()

                return
            }

            if (!root.controller.lastAnswerCorrect || correctAdvanceDelay.running)
            {
                return
            }

            correctAdvanceDelay.start()
        }
    }

    MobileExerciseShell
    {
        id: shell

        anchors.fill: parent
        visible: !(root.finished && root.showComplete)

        exerciseName: root.controller.title.length > 0 ? root.controller.title : qsTr("Choose a translation")
        store: root.store
        controller: root.controller
        hasActions: true

        progress: root.controller.exerciseProgress

        questionSideNames: root.store ? root.store.questionSideNames : []
        questionSideIndex: root.controller.sessionSettings.choiceSide
        inSession: root.inSession

        onExitRequested: root.done()
        onEndSessionRequested: root.endSessionRequested()
        cardEditable: root.cardEditable
        onEditCardRequested: root.editCardRequested()

        showVoiceOption: root.store !== null && root.controller.voiceSupported
        voiceOn: root.controller.voiceRequested
        onVoiceToggled: root.controller.sessionSettings.choiceVoice = !root.controller.sessionSettings.choiceVoice

        onRestartRequested:
        {
            if (root.inSession)
            {
                root.restartRequested()
            }
            else
            {
                root.controller.restart()
            }
        }

        onCompleteRequested: root.completeRequested()
        onQuestionSideChosen: function(index) { root.controller.sessionSettings.choiceSide = index }

        actions: MobileSessionButton
        {
            label: qsTr("Continue")
            interactive: !root.finished && root.controller.answered

            onClicked: root.advance()
        }

        MobileContentArea
        {
            posText: root.controller.partOfSpeech
            posVisible: !root.controller.finished
            counterText: qsTr("%1 / %2")
                .arg(root.controller.currentSessionCount)
                .arg(root.controller.maxSessionCount)

            primaryContent: [
                MobilePlayButton
                {
                    visible: root.voiceHidden
                    playing: root.controller.speaking

                    onClicked: root.controller.replay()
                },
                MobilePromptText
                {
                    visible: !root.voiceHidden
                    text: root.controller.prompt
                    gender: root.controller.headwordGender
                    verbGroup: root.controller.headwordVerbGroup
                    maximumSize: root.controller.showsTargetLanguage
                        ? AppMetrics.sessionWordSize
                        : AppMetrics.sessionTranslationSize
                },
                MobileGhostLink
                {
                    interactive: !root.controller.finished && !root.controller.answered

                    onClicked: root.controller.skip()
                }
            ]

            secondaryContent: MobileOptionGrid
            {
                id: optionGrid

                Layout.fillWidth: true

                rowSpacing: AppMetrics.gapSmall
                columnSpacing: AppMetrics.gapSmall

                cardModel: root.controller.options
                verbGroups: root.controller.optionVerbGroups
                interactive: !root.controller.finished && !root.controller.answered

                borderColours: root.optionBorderColours

                onOptionClicked: function(index) { root.controller.choose(index) }
            }
        }
    }

    MouseArea
    {
        anchors.fill: parent
        enabled: correctAdvanceDelay.running

        onClicked:
        {
            root.advance()
        }
    }

    readonly property var optionBorderColours:
    {
        const colours = []

        for (let i = 0; i < root.controller.options.length; i++)
        {
            if (root.controller.finished || !root.controller.answered)
            {
                colours.push(AppTheme.colourBorder)
                continue
            }

            if (i === root.controller.correctOptionIndex)
            {
                colours.push(AppTheme.colourCorrect)
                continue
            }

            colours.push(i === root.controller.chosenOptionIndex
                ? AppTheme.colourWrong
                : AppTheme.colourBorder)
        }

        return colours
    }

    MobileExerciseCompleteScreen
    {
        anchors.fill: parent
        visible: root.finished && root.showComplete

        exerciseName: root.controller.title.length > 0 ? root.controller.title : qsTr("Choose a translation")

        wordCount: root.controller.currentSessionCount
        elapsedSeconds: root.controller.elapsedSeconds

        onDoneClicked: root.done()
    }
}
