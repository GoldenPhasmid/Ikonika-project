import QtQuick
import QtQuick.Layouts

Item
{
    id: root

    property var controller: FlashcardController {}

    /** Carries the speech setting, so muting inside a session is the same switch as the one in
        settings rather than a second one that disagrees with it. */
    property var store: null

    property bool showComplete: true
    property bool inSession: false
    property bool cardEditable: false
    property bool sentenceText: false

    property bool reachedEnd: false
    readonly property bool finished: root.reachedEnd && !root.controller.canGoNext

    signal done()
    signal endSessionRequested()
    signal editCardRequested()
    signal restartRequested()
    signal completeRequested()
    signal wordHeld(string word)
    signal wordTapped(string word, string sentence)

    function advance()
    {
        shell.swap(function()
        {
            root.controller.next()

            if (!root.controller.canGoNext)
            {
                root.reachedEnd = true
            }
        })
    }

    MobileExerciseShell
    {
        id: shell

        anchors.fill: parent
        visible: !(root.finished && root.showComplete)

        exerciseName: qsTr("Flashcard")
        store: root.store
        controller: root.controller

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
            root.reachedEnd = false

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
        onQuestionSideChosen: function(index) { if (root.store) root.store.questionSide = index }

        MobileContentArea
        {
            posText: root.controller.cardMeta
            posVisible: root.controller.cardMeta.length > 0
            counterText: qsTr("%1 / %2")
                .arg(root.controller.currentSessionCount)
                .arg(root.controller.maxSessionCount)

            primaryContent: [
                MobilePromptText
                {
                    visible: !root.sentenceText
                    gender: root.controller.headwordGender
                    verbGroup: root.controller.headwordVerbGroup
                    text: root.controller.wordMarkup.length > 0 ? root.controller.wordMarkup : root.controller.prompt
                    textFormat: root.controller.wordMarkup.length > 0 ? Text.StyledText : Text.PlainText
                    linkColor: AppTheme.colourAccent
                },
                Text
                {
                    objectName: "flashcard_phonetic"

                    visible: !root.sentenceText
                    text: root.controller.phonetic.length > 0 ? root.controller.phonetic : " "
                    font.pixelSize: AppMetrics.fontTitle
                    font.family: AppMetrics.fontFamily
                    color: AppTheme.colourMuted
                    Layout.fillWidth: true
                    Layout.topMargin: -13
                    horizontalAlignment: Text.AlignHCenter
                },
                MobileSentenceText
                {
                    id: sentenceLabel

                    visible: root.sentenceText
                    sentence: root.sentenceText ? root.controller.prompt : ""
                    holdEnabled: false
                }
            ]

            secondaryContent: [
                Text
                {
                    text: root.controller.answer
                    font.pixelSize: root.sentenceText ? AppMetrics.sessionOptionSize
                                                      : AppMetrics.sessionTranslationSize
                    font.weight: Font.Medium
                    font.family: AppMetrics.fontFamily
                    color: !root.controller.showsTargetLanguage ? AppTheme.verbGroupColours[root.controller.targetVerbGroup] ?? AppTheme.colourText : AppTheme.colourText
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                },
                Text
                {
                    visible: root.controller.definition.length > 0
                    text: root.controller.definition
                    font.pixelSize: AppMetrics.sessionDefinitionSize
                    font.family: AppMetrics.fontFamily
                    color: AppTheme.colourMuted
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                },
                Item
                {
                    Layout.fillHeight: true
                },
                MobileExampleBlock
                {
                    id: exampleBlock

                    sentence: root.sentenceText ? "" : root.controller.example
                    translation: root.controller.exampleTranslation
                }
            ]

            secondaryFillsArea: true

            MouseArea
            {
                id: cardArea

                anchors.fill: parent

                onClicked: function(mouse)
                {
                    if (root.sentenceText && mouse.wasHeld)
                    {
                        return
                    }

                    const word = exampleBlock.wordUnder(cardArea, mouse.x, mouse.y)

                    if (word.length > 0)
                    {
                        root.wordTapped(word, exampleBlock.sentence)

                        return
                    }

                    root.advance()
                }

                onPressAndHold: function(mouse)
                {
                    if (!root.sentenceText)
                    {
                        return
                    }

                    const point = sentenceLabel.mapFromItem(cardArea, mouse.x, mouse.y)
                    const word = sentenceLabel.wordAt(point.x, point.y)

                    if (word.length > 0)
                    {
                        root.wordHeld(word)
                    }
                }
            }
        }

        actions: MobileButtonPair
        {
            leftLabel: qsTr("Previous")
            rightLabel: root.controller.canGoNext ? qsTr("Next") : qsTr("Finish")

            leftArrow: "left"
            rightArrow: root.controller.canGoNext ? "right" : ""

            leftEnabled: root.controller ? root.controller.canGoPrevious : false
            rightEnabled: true

            onLeftClicked: shell.swapBack(function() { root.controller.previous() })
            onRightClicked: root.advance()
        }
    }

    MobileExerciseCompleteScreen
    {
        anchors.fill: parent
        visible: root.finished && root.showComplete

        exerciseName: qsTr("Flashcard")

        wordCount: root.controller.currentSessionCount
        elapsedSeconds: root.controller.elapsedSeconds

        onDoneClicked: root.done()
    }
}
