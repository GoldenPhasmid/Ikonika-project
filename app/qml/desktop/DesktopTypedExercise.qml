import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item
{
    id: root

    property var controller: null
    property var store: null
    property bool sentenceText: false

    readonly property string kind: "typed"
    readonly property Item surface: card.surface
    readonly property string gapMark: "___"
    readonly property bool editable: !root.sentenceText

    signal wordClicked(string word, string sentence)

    readonly property bool listening: root.controller instanceof AudioTranscriptionController

    readonly property string title: root.listening ? qsTr("Write what you hear") : qsTr("Write the translation")
    readonly property bool voiceSupported: false
    readonly property bool voiceOn: false

    readonly property bool revealed: !!root.controller && !root.controller.finished && root.controller.answered
    readonly property bool awaiting: !!root.controller && !root.controller.finished && !root.controller.answered
    readonly property bool retyping: root.revealed && root.controller.retypePending
    readonly property bool formsHidden: !root.revealed
    property bool retyped: false

    readonly property bool sentenceMode: root.sentenceText || (!!root.controller && !!root.controller.sentencePrompt)
    readonly property bool hasMarkup: !!root.controller && root.controller.lastWordMarkup.length > 0
    readonly property bool hasHeading: !root.sentenceText && !root.listening && !!root.controller && root.controller.promptHeading.length > 0
    readonly property bool answerInPrompt: root.hasHeading && !root.sentenceMode && root.revealed && root.hasMarkup
    readonly property bool answerOnLine: !root.listening && !root.sentenceMode && !root.hasHeading
    readonly property bool answerIsTarget: root.answerOnLine && root.revealed && !root.hasMarkup && !root.controller.showsTargetLanguage
    readonly property bool longPrompt: !!root.controller && !root.sentenceMode && root.controller.prompt.trim().split(/\s+/).length > 2
    readonly property string typedText: answerField.text + answerField.preeditText
    readonly property bool gapFilling: root.awaiting && root.sentenceMode && !root.listening && root.typedText.length > 0

    function toggleVoice()
    {
    }

    function focusAnswer()
    {
        if (root.awaiting || root.retyping)
        {
            answerField.forceActiveFocus()
        }
    }

    function submitOrAdvance()
    {
        if (root.retyping)
        {
            root.controller.submitAnswer(answerField.text + answerField.preeditText)

            return
        }

        if (root.revealed)
        {
            root.advanceToNext()

            return
        }

        if (root.awaiting)
        {
            root.controller.submitAnswer(answerField.text + answerField.preeditText)
        }
    }

    function advanceToNext()
    {
        correctAdvanceDelay.stop()
        root.retyped = false

        card.swap(function()
        {
            answerField.clear()
            root.controller.advance()
            Qt.callLater(root.focusAnswer)
        })
    }

    function playVoice()
    {
        if (promptLabel.visible && promptLabel.speakable)
        {
            promptLabel.speak()

            return
        }

        if (revealedLabel.visible)
        {
            revealedLabel.speak()

            return
        }

        root.controller.replay()
    }

    function dontKnow()
    {
        if (root.awaiting)
        {
            root.controller.skip()
        }
    }

    function handleKey(event)
    {
        if (!root.controller)
        {
            return false
        }

        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
        {
            root.submitOrAdvance()

            return true
        }

        return false
    }

    Timer
    {
        id: correctAdvanceDelay

        interval: AppMetrics.exerciseAdvanceDelayCorrect
        onTriggered: root.advanceToNext()
    }

    Connections
    {
        target: root.controller

        function onAnswerGraded(correct)
        {
            if (correct)
            {
                card.armMasteryPop()

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
                correctAdvanceDelay.stop()

                return
            }

            if (root.controller.retypePending)
            {
                Qt.callLater(root.focusAnswer)

                return
            }

            if (!root.sentenceMode && (root.controller.lastAnswerCorrect || root.retyped) && !correctAdvanceDelay.running)
            {
                correctAdvanceDelay.start()
            }
        }
    }

    onControllerChanged: Qt.callLater(root.focusAnswer)
    onVisibleChanged: if (root.visible) Qt.callLater(root.focusAnswer)

    DesktopSentenceFit
    {
        id: sentenceFit

        width: card.surface.width - AppMetrics.contentPadding * 2
        availableHeight: card.primaryHeight
        reservedHeight: root.hasHeading ? AppMetrics.sentenceNativeSize * AppMetrics.answerLineCount + AppMetrics.gapSmall : 0
        foreignText: root.controller && !root.controller.finished && root.sentenceMode ? (root.listening ? root.controller.prompt : root.controller.answer) : ""
        nativeText: root.controller && !root.controller.finished && root.sentenceMode ? (root.hasHeading ? root.controller.promptHeading
            : (root.listening ? root.controller.answer : "")) : ""
    }

    DesktopExerciseCard
    {
        id: card

        anchors.fill: parent

        fillSecondary: true
        primarySpeaks: !root.listening && ((promptLabel.visible && promptLabel.speakable) || revealedLabel.visible)
        onPrimaryClicked: root.playVoice()
        tags: root.sentenceMode ? [] : card.wordTags(root.controller)
        counterText: root.controller ? qsTr("%1 / %2").arg(root.controller.currentSessionCount).arg(root.controller.maxSessionCount) : ""
        masteryTarget: root.controller ? root.controller.masteryTarget : 0
        masteryFilled: root.controller ? root.controller.cardMasteryCount : 0

        primary: [
            DesktopLabel
            {
                visible: root.hasHeading
                Layout.fillWidth: true
                text: root.hasHeading ? root.controller.promptHeading : ""
                font.pixelSize: root.sentenceMode ? sentenceFit.nativeSize : AppMetrics.sessionTranslationSize
                color: AppTheme.colourSubtle
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            },
            DesktopPromptLabel
            {
                id: promptLabel

                visible: !root.listening && !root.sentenceMode
                maximumSize: root.longPrompt ? AppMetrics.fontHeadline : AppMetrics.sessionWordSize
                speakable: !root.answerInPrompt && !!(root.controller && root.controller.showsTargetLanguage)
                gender: root.controller && !root.answerInPrompt ? root.controller.headwordGender : -1
                verbGroup: root.controller && !root.answerInPrompt ? root.controller.headwordVerbGroup : ""
                text: !root.controller ? "" : (root.answerInPrompt ? root.controller.lastWordMarkup
                                                                    : promptLabel.withArticle(root.controller.prompt, root.controller.promptArticle))
                textFormat: Text.StyledText

                Accessible.role: Accessible.Heading
                Accessible.name: root.controller ? root.controller.prompt : ""

                onSpeakRequested: root.controller.speakPrompt()
            },
            DesktopPromptLabel
            {
                id: revealedLabel

                visible: root.listening && root.revealed && !root.sentenceMode
                speakable: true
                Layout.fillHeight: false
                Layout.minimumHeight: AppMetrics.waveformButtonHeight
                Layout.preferredHeight: AppMetrics.waveformButtonHeight
                Layout.maximumHeight: AppMetrics.waveformButtonHeight
                gender: root.controller && !root.hasMarkup ? root.controller.targetGender : -1
                verbGroup: root.controller && !root.hasMarkup ? root.controller.targetVerbGroup : ""
                text: !root.controller ? "" : (root.hasMarkup ? root.controller.lastWordMarkup
                                                              : revealedLabel.withArticle(root.controller.lastExpectedAnswer || "", root.controller.targetArticle))
                textFormat: Text.StyledText

                onSpeakRequested: root.controller.replay()
            },
            DesktopPlayButton
            {
                id: listenButton

                visible: !root.revealed && root.listening
                Layout.alignment: Qt.AlignHCenter
                playing: root.controller ? root.controller.speaking : false
                keyHint: qsTr("Ctrl+Space")

                onClicked: root.controller.replay()
            },
            DesktopSentenceText
            {
                maximumSize: sentenceFit.foreignSize
                Layout.minimumHeight: sentenceMeasure.implicitHeight
                visible: root.sentenceMode && (!root.listening || root.revealed)
                sentence:
                {
                    if (!root.controller || !root.sentenceMode)
                    {
                        return ""
                    }

                    if (root.listening)
                    {
                        return root.controller.lastExpectedAnswer || ""
                    }

                    if (root.revealed)
                    {
                        return root.controller.lastRevealedAnswer
                    }

                    return root.gapFilling ? root.controller.prompt.replace(root.gapMark, root.typedText) : root.controller.prompt
                }
                gapStart: (root.revealed && !root.listening) || root.gapFilling ? root.controller.prompt.indexOf(root.gapMark) : -1
                gapLength:
                {
                    if (root.gapFilling)
                    {
                        return root.typedText.length
                    }

                    return root.revealed && !root.listening
                        ? root.controller.lastRevealedAnswer.length - (root.controller.prompt.length - root.gapMark.length) : 0
                }

                onWordClicked: function(word, sentence) { root.wordClicked(word, sentence) }
            },
            DesktopLabel
            {
                opacity: root.revealed ? 1 : 0
                Accessible.ignored: !root.revealed
                Layout.fillWidth: true
                Layout.minimumHeight: answerLines.lineSpacing * AppMetrics.answerLineCount
                Layout.preferredHeight: Layout.minimumHeight
                Layout.maximumHeight: Layout.minimumHeight
                text:
                {
                    if (!root.controller || !root.revealed)
                    {
                        return ""
                    }

                    if (!root.answerOnLine)
                    {
                        return root.controller.lastCardTranslation
                    }

                    if (root.hasMarkup)
                    {
                        return root.controller.lastWordMarkup
                    }

                    return root.answerIsTarget ? promptLabel.withArticle(root.controller.lastRevealedAnswer, root.controller.targetArticle)
                                               : root.controller.lastRevealedAnswer
                }
                textFormat: root.answerOnLine && (root.hasMarkup || root.answerIsTarget) ? Text.StyledText : Text.PlainText
                font.pixelSize: root.sentenceMode ? sentenceFit.nativeSize : AppMetrics.fontLead
                color:
                {
                    if (root.answerIsTarget && AppTheme.verbGroupColours[root.controller.targetVerbGroup])
                    {
                        return AppTheme.verbGroupColours[root.controller.targetVerbGroup]
                    }
                    if (root.answerIsTarget && root.controller.targetGender >= 0)
                    {
                        return AppTheme.genderColours[root.controller.targetGender]
                    }

                    return root.answerOnLine && !!root.controller && !root.controller.lastAnswerCorrect && !root.retyped
                        ? AppTheme.colourCorrect : AppTheme.colourSubtle
                }
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                maximumLineCount: AppMetrics.answerLineCount
                elide: Text.ElideRight
            }
        ]

        secondary: [
            DesktopAnswerField
            {
                id: answerField

                objectName: "exercise_answer"

                placeholderText: !root.listening && root.controller ? root.controller.answerHint : ""
                readOnly: !(root.awaiting || root.retyping)
                marked: root.revealed && (!root.retyping || answerField.holdingMiss)
                correct: !!root.controller && (root.controller.lastAnswerCorrect || root.retyped)

                Keys.onReturnPressed: root.submitOrAdvance()
                Keys.onEnterPressed: root.submitOrAdvance()
            },
            Item
            {
                Layout.fillHeight: true
            },
            ColumnLayout
            {
                id: exampleBlock

                readonly property string sentence: root.revealed && !root.sentenceMode ? root.controller.example : ""
                property bool expanded: false

                onSentenceChanged: expanded = false

                visible: sentence.length > 0
                Layout.fillWidth: true
                spacing: 0

                Rectangle
                {
                    Layout.fillWidth: true
                    Layout.preferredHeight: exampleBlock.expanded ? exampleDetails.implicitHeight + AppMetrics.gapLarge * 2 : 0
                    opacity: exampleBlock.expanded ? 1 : 0
                    clip: true
                    radius: AppMetrics.radiusMedium
                    color: AppTheme.colourWell

                    Behavior on Layout.preferredHeight
                    {
                        NumberAnimation { duration: AppMetrics.motionSlow; easing.type: AppMetrics.motionEasing }
                    }

                    Behavior on opacity
                    {
                        NumberAnimation { duration: AppMetrics.motionSlow; easing.type: AppMetrics.motionEasing }
                    }

                    DesktopRevealDetails
                    {
                        id: exampleDetails

                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: AppMetrics.gapLarge
                        controller: exampleBlock.sentence.length > 0 ? root.controller : null

                        onWordClicked: function(word, sentence) { root.wordClicked(word, sentence) }
                    }
                }

                DesktopIconButton
                {
                    objectName: "exercise_example_toggle"

                    Layout.alignment: Qt.AlignHCenter
                    iconName: exampleBlock.expanded ? "chevron_down" : "chevron_up"
                    iconColour: AppTheme.colourMuted
                    text: exampleBlock.expanded ? qsTr("Hide example") : qsTr("Show example")

                    onClicked:
                    {
                        exampleBlock.expanded = !exampleBlock.expanded
                        console.info("Example block", exampleBlock.expanded ? "expanded" : "collapsed")
                    }
                }
            }
        ]

        controls: [
            DesktopButton
            {
                objectName: "exercise_dont_know"

                visible: root.awaiting
                Layout.preferredWidth: AppMetrics.actionButtonWidth
                large: true
                tone: "secondary"
                text: qsTr("Don't know")
                keyHint: qsTr("Ctrl+D")

                onClicked: root.dontKnow()
            },
            DesktopButton
            {
                objectName: "exercise_continue"

                Layout.preferredWidth: AppMetrics.actionButtonWidth
                large: true
                tone: "primary"
                text: root.revealed && !root.retyping ? qsTr("Continue") : qsTr("Check")
                keyHint: qsTr("Enter")
                enabled: (root.revealed && !root.retyping)
                    || ((root.awaiting || root.retyping) && (answerField.text + answerField.preeditText).trim().length > 0)

                onClicked: root.submitOrAdvance()
            }
        ]
    }

    FontMetrics
    {
        id: answerLines

        font.family: AppMetrics.fontFamily
        font.pixelSize: root.sentenceMode ? sentenceFit.nativeSize : AppMetrics.fontLead
    }

    Text
    {
        id: sentenceMeasure

        visible: false
        width: sentenceFit.width
        text: sentenceFit.foreignText
        textFormat: Text.PlainText
        font.family: AppMetrics.fontFamily
        font.pixelSize: sentenceFit.foreignSize
        font.weight: Font.Medium
        wrapMode: Text.WordWrap
    }

    MouseArea
    {
        anchors.fill: parent
        enabled: correctAdvanceDelay.running

        onClicked: root.advanceToNext()
    }
}
