import QtQuick
import QtQuick.Layouts

Item
{
    id: root

    property var controller: AudioTranscriptionController {}

    property var store: null

    property bool showComplete: true
    property bool inSession: false
    property bool cardEditable: false
    property string exerciseName: qsTr("Compile word audio")
    property bool sentenceText: false
    property bool manualAdvance: false

    readonly property bool revealed: !root.controller.finished && root.controller.answered
    readonly property bool retyping: root.revealed && root.controller.retypePending
    readonly property bool typing: !root.controller.finished && (!root.controller.answered || root.retyping)
    property bool retyped: false

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

            if (!root.visible || !(root.controller instanceof AudioTranscriptionController))
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
        controller: root.controller
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

            primaryContent: [
                MobilePlayButton
                {
                    visible: !root.sentenceText && !root.revealed
                    playing: root.controller.speaking

                    onClicked: root.controller.replay()
                },
                MobileSentenceText
                {
                    visible: root.sentenceText && root.revealed
                    sentence: root.sentenceText ? root.controller.lastExpectedAnswer : ""
                    tapEnabled: true

                    onWordHeld: function(word) { root.wordHeld(word) }
                    onWordTapped: function(word, sentence) { root.wordTapped(word, sentence) }
                },
                ColumnLayout
                {
                    visible: !root.sentenceText && root.revealed
                    Layout.fillWidth: true
                    spacing: AppMetrics.gapTight

                    MobilePromptText
                    {
                        id: revealedWord

                        readonly property bool hasMarkup: !!root.controller && root.controller.lastWordMarkup.length > 0

                        text: !root.controller ? ""
                            : revealedWord.hasMarkup ? root.controller.lastWordMarkup
                            : revealedWord.withArticle(root.controller.lastExpectedAnswer, root.controller.targetArticle)
                        textFormat: Text.StyledText
                        linkColor: AppTheme.colourAccent
                        maximumSize: AppMetrics.sessionAnswerSize
                        weight: Font.DemiBold
                        gender: root.controller && !revealedWord.hasMarkup ? root.controller.targetGender : -1
                        verbGroup: root.controller && !revealedWord.hasMarkup ? root.controller.targetVerbGroup : ""
                        wrapMode: Text.WordWrap
                    }

                    Text
                    {
                        text: root.controller ? root.controller.lastCardTranslation : ""
                        font.pixelSize: AppMetrics.sessionTranslationSize
                        font.family: AppMetrics.fontFamily
                        color: AppTheme.colourSubtle
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }
                },
                MobileSentenceAudioSlot
                {
                    visible: root.sentenceText
                    revealed: root.revealed
                    playing: root.controller.speaking
                    translation: root.controller ? root.controller.lastCardTranslation : ""

                    onReplayRequested: root.controller.replay()
                }
            ]

            secondaryContent: [
                MobileAnswerField
                {
                    id: answerField

                    placeholderText: qsTr("Type what you hear...")

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
                Item
                {
                    Layout.fillHeight: true
                },
                MobileExampleBlock
                {
                    sentence: root.revealed && !root.controller.lastAnswerCorrect && !root.sentenceText ? root.controller.example : ""
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
