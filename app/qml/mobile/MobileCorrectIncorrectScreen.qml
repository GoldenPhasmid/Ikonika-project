import QtQuick
import QtQuick.Layouts

Item
{
    id: root

    property var controller: CorrectIncorrectController {}

    property var store: null

    property bool showComplete: true
    property bool inSession: false
    property bool cardEditable: false
    property bool showMute: true
    property bool showOptions: true

    property string exerciseName: qsTr("Correct / Incorrect")
    property string incorrectLabel: qsTr("Incorrect")
    property string correctLabel: qsTr("Correct")

    readonly property bool finished: root.controller.finished

    signal done()
    signal endSessionRequested()
    signal editCardRequested()
    signal restartRequested()
    signal completeRequested()
    signal wordTapped(string word, string sentence)

    MobileExerciseShell
    {
        id: shell

        anchors.fill: parent
        visible: !(root.finished && root.showComplete)

        exerciseName: root.exerciseName
        showMute: root.showMute
        showOptions: root.showOptions
        store: root.store
        controller: root.controller

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

            primaryContent: [
                MobilePromptText
                {
                    text: root.controller.prompt
                    gender: root.controller.headwordGender
                    verbGroup: root.controller.headwordVerbGroup
                },
                Text
                {
                    objectName: "review_phonetic"

                    text: root.controller.showsTargetLanguage && root.controller.phonetic.length > 0
                        ? root.controller.phonetic : " "
                    font.pixelSize: AppMetrics.fontTitle
                    font.family: AppMetrics.fontFamily
                    color: AppTheme.colourMuted
                    Layout.fillWidth: true
                    Layout.topMargin: -13
                    horizontalAlignment: Text.AlignHCenter
                }
            ]

            secondaryContent: [
                Text
                {
                    text: root.controller.answered
                        ? root.controller.lastRevealedAnswer
                        : "?"
                    font.pixelSize: AppMetrics.sessionAnswerSize
                    font.weight: Font.Medium
                    font.family: AppMetrics.fontFamily
                    color: root.controller.answered
                        ? (!root.controller.showsTargetLanguage ? AppTheme.verbGroupColours[root.controller.targetVerbGroup] ?? AppTheme.colourText : AppTheme.colourText)
                        : AppTheme.colourMuted
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                },
                Text
                {
                    visible: root.controller.answered && root.controller.definition.length > 0
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

                    sentence: root.controller.answered ? root.controller.example : ""
                    translation: root.controller.exampleTranslation

                    onWordTapped: function(word, sentence) { root.wordTapped(word, sentence) }
                }
            ]

            secondaryFillsArea: true

            MouseArea
            {
                anchors.fill: parent
                enabled: !root.controller.answered
                onClicked: root.controller.reveal()
            }
        }

        actions: [
            MobileSessionButton
            {
                label: qsTr("Reveal")
                visible: !root.controller.answered

                onClicked: root.controller.reveal()
            },
            MobileButtonPair
            {
                visible: root.controller.answered

                leftLabel: root.incorrectLabel
                rightLabel: root.correctLabel

                onLeftClicked: shell.swap(function() { root.controller.answerNo() })
                onRightClicked: shell.swap(function() { root.controller.answerYes() })
            }
        ]
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
