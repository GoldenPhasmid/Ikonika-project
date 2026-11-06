import QtQuick
import QtQuick.Layouts

Item
{
    id: root

    property var controller: WrittenProductionController {}

    property var store: null

    property bool showComplete: true
    property bool inSession: false
    property bool cardEditable: false
    property string exerciseName: qsTr("Compile word")
    property bool sentenceText: false
    property bool manualAdvance: false

    readonly property bool revealed: !root.controller.finished && root.controller.answered
    readonly property bool retyping: root.revealed && root.controller.retypePending
    readonly property bool typing: !root.controller.finished && (!root.controller.answered || root.retyping)
    property bool retyped: false
    readonly property bool sentenceMode: root.sentenceText || !!root.controller.sentencePrompt
    readonly property bool sentenceRevealed: root.sentenceMode && root.revealed
    readonly property bool hasMarkup: root.controller.lastWordMarkup.length > 0
    readonly property bool hasHeading: root.controller.promptHeading.length > 0
    readonly property bool answerInPrompt: root.hasHeading && !root.sentenceMode && root.revealed && root.hasMarkup
    readonly property string gapMark: "___"

    readonly property bool finished: root.controller.finished

    signal done()
    signal endSessionRequested()
    signal editCardRequested()
    signal restartRequested()
    signal completeRequested()
    signal wordHeld(string word)
    signal wordTapped(string word, string sentence)

    function submitOrAdvance()
    {
        if (root.retyping)
        {
            root.controller.submitAnswer(answerField.text)

            return
        }

        if ((!root.controller.finished && root.controller.answered))
        {
            root.advanceToNext()

            return
        }

        if ((!root.controller.finished && !root.controller.answered))
        {
            root.controller.submitAnswer(answerField.text)
        }
    }

    function advanceToNext()
    {
        root.stopAutoAdvance()
        root.retyped = false

        shell.swap(function()
        {
            answerField.clear()
            Qt.inputMethod.reset()
            root.controller.advance()

            if (!root.visible || !(root.controller instanceof WrittenProductionController))
            {
                return
            }

            answerField.forceActiveFocus()
            Qt.inputMethod.show()
        })
    }

    function focusAnswer()
    {
        if (!root.visible || !root.typing)
        {
            return
        }

        answerField.forceActiveFocus()
        Qt.inputMethod.show()
    }

    onVisibleChanged: if (root.visible) Qt.callLater(root.focusAnswer)

    function stopAutoAdvance()
    {
        correctAdvanceDelay.stop()
    }

    Timer
    {
        id: correctAdvanceDelay

        interval: AppMetrics.exerciseAdvanceDelayCorrect
        onTriggered: root.advanceToNext()
    }

    Timer
    {
        id: retypeFocusDelay

        interval: AppMetrics.motionSlow
        onTriggered: root.focusAnswer()
    }

    Connections
    {
        target: root.controller

        function onAnswerGraded(correct)
        {
            if (correct)
            {
                contentArea.armMasteryPop()

                return
            }

            answerField.shake()
            answerField.holdMiss()
        }

        function onRetypeGraded(correct)
        {
            if (correct)
            {
                root.retyped = true

                return
            }

            answerField.shake()
            answerField.holdMiss()
        }

        function onStateChanged()
        {
            if (root.controller.finished || !root.controller.answered)
            {
                root.stopAutoAdvance()

                return
            }

            if (root.controller.retypePending)
            {
                root.focusAnswer()
                retypeFocusDelay.restart()

                return
            }

            Qt.inputMethod.hide()

            if (root.manualAdvance || !(root.controller.lastAnswerCorrect || root.retyped) || correctAdvanceDelay.running)
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

        exerciseName: root.exerciseName
        store: root.store
        hasActions: true

        progress: root.controller.exerciseProgress

        questionSideNames: root.store ? root.store.questionSideNames : []
        questionSideIndex: root.store ? root.store.questionSide : 0
        inSession: root.inSession

        onExitRequested: root.done()
        onEndSessionRequested: root.endSessionRequested()
        cardEditable: root.cardEditable
        onEditCardRequested: root.editCardRequested()

        onRestartRequested:
        {
            if (root.inSession)
            {
                root.restartRequested()
            }
            else
            {
                root.controller.startSession()
            }
        }

        onCompleteRequested: root.completeRequested()
        onQuestionSideChosen: function(index) { if (root.store) root.store.questionSide = index }

        actions: MobileSessionButton
        {
            label: qsTr("Continue")
            interactive: root.revealed && !root.retyping

            onClicked: root.advanceToNext()
        }

        MobileContentArea
        {
            id: contentArea

            posText: root.controller.partOfSpeech
            posVisible: !root.controller.finished
            counterText: qsTr("%1 / %2")
                .arg(root.controller.currentSessionCount)
                .arg(root.controller.maxSessionCount)
            masteryTarget: root.controller.masteryTarget
            masteryFilled: root.controller.cardMasteryCount
            secondaryFillsArea: true
            primaryFitsContent: root.sentenceMode

            primaryContent: [
                Text
                {
                    visible: root.hasHeading
                    text: root.controller.promptHeading
                    font.pixelSize: root.sentenceMode ? AppMetrics.sessionTranslationSize : AppMetrics.sessionAnswerSize
                    font.weight: Font.Medium
                    font.family: AppMetrics.fontFamily
                    color: root.sentenceMode ? AppTheme.colourSubtle : AppTheme.colourText
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                },
                MobileSentenceText
                {
                    visible: root.sentenceMode
                    sentence: !root.sentenceMode ? ""
                        : (root.revealed ? root.controller.lastRevealedAnswer : root.controller.prompt)
                    gapStart: root.sentenceRevealed ? root.controller.prompt.indexOf(root.gapMark) : -1
                    gapLength: root.sentenceRevealed
                        ? root.controller.lastRevealedAnswer.length
                          - (root.controller.prompt.length - root.gapMark.length)
                        : 0
                    tapEnabled: true

                    onWordHeld: function(word) { root.wordHeld(word) }
                    onWordTapped: function(word, sentence) { root.wordTapped(word, sentence) }
                },
                MobilePromptText
                {
                    visible: !root.sentenceMode
                    text: root.answerInPrompt ? root.controller.lastWordMarkup : root.controller.prompt
                    verbGroup: !root.answerInPrompt ? root.controller.headwordVerbGroup : ""
                    textFormat: root.answerInPrompt ? Text.StyledText : Text.PlainText
                    linkColor: AppTheme.colourAccent
                    maximumSize: root.controller.showsTargetLanguage && !root.hasHeading
                        ? AppMetrics.sessionWordSize : AppMetrics.sessionTranslationSize
                    weight: root.hasHeading ? Font.Medium : Font.DemiBold
                },
                MobileSentenceAudioSlot
                {
                    visible: root.sentenceMode
                    revealed: root.revealed
                    canPlay: !!root.controller.hasPromptSpeech
                    playing: root.controller.speaking
                    translation: root.controller.lastCardTranslation

                    onReplayRequested: root.controller.replay()
                },
                MobilePromptText
                {
                    id: revealedAnswer

                    readonly property bool answerIsTarget: !root.hasHeading && !root.hasMarkup
                        && !root.controller.showsTargetLanguage && root.revealed

                    visible: !root.sentenceMode
                    text:
                    {
                        if (root.hasHeading)
                        {
                            return root.controller.lastCardTranslation
                        }

                        if (root.hasMarkup)
                        {
                            return root.controller.lastWordMarkup
                        }

                        return revealedAnswer.answerIsTarget
                            ? revealedAnswer.withArticle(root.controller.lastRevealedAnswer, root.controller.targetArticle)
                            : root.controller.lastRevealedAnswer
                    }
                    textFormat: !root.hasHeading && (root.hasMarkup || revealedAnswer.answerIsTarget) ? Text.StyledText : Text.PlainText
                    linkColor: AppTheme.colourAccent
                    opacity: (!root.controller.finished && root.controller.answered) ? 1.0 : 0.0
                    maximumSize: root.controller.showsTargetLanguage || root.hasHeading
                        ? AppMetrics.sessionTranslationSize : AppMetrics.sessionWordSize
                    gender: revealedAnswer.answerIsTarget ? root.controller.targetGender : -1
                    verbGroup: revealedAnswer.answerIsTarget ? root.controller.targetVerbGroup : ""
                    baseColour: AppTheme.colourSubtle

                    Behavior on opacity { NumberAnimation { duration: AppMetrics.motionFast } }
                }
            ]

            secondaryContent: [
                MobileAnswerField
                {
                    id: answerField

                    placeholderText: root.controller.answerHint.length > 0 ? root.controller.answerHint
                                                                          : qsTr("Type translation...")
                    focus: root.visible && root.typing
                    readOnly: !root.typing

                    marked: root.revealed && (!root.retyping || answerField.holdingMiss)
                    correct: root.controller.lastAnswerCorrect || root.retyped

                    onAccepted: root.submitOrAdvance()
                },
                MobileGhostLink
                {
                    interactive: (!root.controller.finished && !root.controller.answered)

                    onClicked: root.controller.skip()
                },
                MobileFormsTable
                {
                    visible: root.revealed && root.controller.forms.length > 0
                    Layout.fillWidth: true
                    Layout.topMargin: AppMetrics.gapLarge
                    forms: root.controller.forms
                },
                Item
                {
                    Layout.fillHeight: true
                },
                MobileExampleBlock
                {
                    collapsible: true
                    sentence: root.revealed && !root.sentenceMode ? root.controller.example : ""
                    translation: root.controller.exampleTranslation

                    onWordTapped: function(word, sentence) { root.wordTapped(word, sentence) }
                }
            ]
        }
    }

    MouseArea
    {
        anchors.fill: parent
        enabled: correctAdvanceDelay.running

        onClicked: root.advanceToNext()
    }

    MobileExerciseCompleteScreen
    {
        anchors.fill: parent
        visible: root.finished && root.showComplete

        exerciseName: root.exerciseName

        wordCount: root.controller.currentSessionCount
        elapsedSeconds: root.controller.elapsedSeconds

        onDoneClicked: root.done()
    }
}
