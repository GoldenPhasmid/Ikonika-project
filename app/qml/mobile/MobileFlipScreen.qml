import QtQuick
import QtQuick.Layouts

Item
{
    id: root

    property var controller: FlipRecallController {}

    property var store: null

    property bool showComplete: true
    property bool inSession: false
    property bool cardEditable: false

    readonly property bool finished: root.controller.finished
    readonly property bool wordShown: root.controller.showsTargetLanguage
    readonly property bool hiddenSideOpen: root.controller.answered
    readonly property string hiddenSideText: root.hiddenSideOpen ? root.controller.lastRevealedAnswer : "?"

    signal done()
    signal endSessionRequested()
    signal editCardRequested()
    signal restartRequested()
    signal completeRequested()
    signal wordTapped(string word, string sentence)

    function reveal()
    {
        if (!root.controller.answered)
        {
            root.controller.advance()
        }
    }

    MobileExerciseShell
    {
        id: shell

        anchors.fill: parent
        visible: !(root.finished && root.showComplete)

        exerciseName: qsTr("Flip")
        store: root.store

        progress: root.controller.exerciseProgress

        questionSideNames: root.store ? root.store.questionSideNames : []
        questionSideIndex: root.store ? root.store.questionSide : 0
        inSession: root.inSession

        onExitRequested: root.done()

        swipeEnabled: !root.finished && root.controller.answered
        onSwiped: function(direction)
        {
            shell.swipeOut(direction > 0 ? function() { root.controller.answerYes() } : function() { root.controller.answerNo() },
                           direction)
        }
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

            primaryContent: MobilePromptText
            {
                text: root.wordShown ? root.controller.prompt : root.hiddenSideText
                gender: root.wordShown ? root.controller.headwordGender : -1
                verbGroup: root.wordShown ? root.controller.headwordVerbGroup : ""
                baseColour: root.wordShown || root.hiddenSideOpen ? AppTheme.colourText : AppTheme.colourMuted
            }

            secondaryContent: [
                Text
                {
                    text: root.wordShown ? root.hiddenSideText : root.controller.prompt
                    font.pixelSize: AppMetrics.sessionTranslationSize
                    font.weight: Font.Medium
                    font.family: AppMetrics.fontFamily
                    color: !root.wordShown && !root.controller.showsTargetLanguage ? AppTheme.verbGroupColours[root.controller.targetVerbGroup] ?? AppTheme.colourText
                        : !root.wordShown || root.hiddenSideOpen ? AppTheme.colourText : AppTheme.colourMuted
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

                    sentence: root.hiddenSideOpen ? root.controller.example : ""
                    translation: root.controller.exampleTranslation

                    onWordTapped: function(word, sentence) { root.wordTapped(word, sentence) }
                }
            ]

            secondaryFillsArea: true

            MouseArea
            {
                anchors.fill: parent
                enabled: !root.controller.answered
                onClicked: root.reveal()
            }
        }

        actions: [
            MobileSessionButton
            {
                label: qsTr("Reveal")
                visible: !root.controller.answered

                onClicked: root.reveal()
            },
            MobileButtonPair
            {
                visible: root.controller.answered

                leftLabel: qsTr("I forgot")
                rightLabel: qsTr("I knew it")

                onLeftClicked: shell.swap(function() { root.controller.answerNo() })
                onRightClicked: shell.swap(function() { root.controller.answerYes() })
            }
        ]
    }

    MobileExerciseCompleteScreen
    {
        anchors.fill: parent
        visible: root.finished && root.showComplete

        exerciseName: qsTr("Flip")

        wordCount: root.controller.currentSessionCount
        elapsedSeconds: root.controller.elapsedSeconds

        onDoneClicked: root.done()
    }
}
