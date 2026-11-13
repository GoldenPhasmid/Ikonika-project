import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item
{
    id: root

    property var controller: null
    property var store: null

    readonly property string kind: "choice"
    readonly property Item surface: card.surface
    readonly property bool editable: true

    readonly property string instruction: !root.controller ? ""
        : root.controller.voicePrompt ? qsTr("Listen and pick")
        : root.controller.showsTargetLanguage ? qsTr("Pick translation")
        : qsTr("Pick the word")
    readonly property bool voiceSupported: !!root.controller && root.controller.voiceSupported
    readonly property bool voiceOn: !!root.controller && root.controller.voiceRequested
    readonly property bool voiceHidden: !!root.controller && root.controller.voicePrompt && !root.controller.answered
    readonly property bool awaiting: !!root.controller && !root.controller.finished && !root.controller.answered

    function toggleVoice()
    {
        if (root.controller)
        {
            root.controller.sessionSettings.choiceVoice = !root.controller.sessionSettings.choiceVoice
        }
    }

    function advance()
    {
        correctAdvanceDelay.stop()
        card.swap(function() { root.controller.advance() })
    }

    function playVoice()
    {
        if (root.controller.voicePrompt)
        {
            root.controller.replay()

            return
        }

        if (promptLabel.speakable)
        {
            promptLabel.speak()
        }
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

        const number = event.key - Qt.Key_1

        if (root.awaiting && number >= 0 && number < root.controller.options.length)
        {
            root.controller.choose(number)

            return true
        }

        if (root.awaiting && event.key === Qt.Key_0)
        {
            root.dontKnow()

            return true
        }

        if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && root.controller.answered)
        {
            root.advance()

            return true
        }

        return false
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
                correctAdvanceDelay.stop()

                return
            }

            if (root.controller.lastAnswerCorrect && !correctAdvanceDelay.running)
            {
                correctAdvanceDelay.start()
            }
        }
    }

    readonly property var stateColours:
    {
        const colours = []

        if (!root.controller)
        {
            return colours
        }

        for (let i = 0; i < root.controller.options.length; i++)
        {
            if (!root.controller.answered)
            {
                colours.push(AppTheme.colourBorder)

                continue
            }

            if (i === root.controller.correctOptionIndex)
            {
                colours.push(AppTheme.colourCorrect)

                continue
            }

            colours.push(i === root.controller.chosenOptionIndex ? AppTheme.colourWrong : AppTheme.colourBorder)
        }

        return colours
    }

    DesktopExerciseCard
    {
        id: card

        anchors.fill: parent

        primarySpeaks: !root.voiceHidden && promptLabel.speakable
        onPrimaryClicked: root.playVoice()
        tags: card.wordTags(root.controller)
        counterText: root.controller ? qsTr("%1 / %2").arg(root.controller.currentSessionCount).arg(root.controller.maxSessionCount) : ""

        primary: [
            Item
            {
                visible: root.voiceHidden
                Layout.fillWidth: true
                Layout.fillHeight: true

                DesktopPlayButton
                {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    playing: root.controller ? root.controller.speaking : false
                    keyHint: qsTr("Ctrl+Space")

                    onClicked: root.controller.replay()
                }
            },
            DesktopPromptLabel
            {
                id: promptLabel

                visible: !root.voiceHidden
                speakable: !!(root.controller && root.controller.showsTargetLanguage)
                gender: root.controller ? root.controller.headwordGender : -1
                verbGroup: root.controller ? root.controller.headwordVerbGroup : ""

                onSpeakRequested: root.controller.speakPrompt()
                text: root.controller ? promptLabel.withArticle(root.controller.prompt, root.controller.promptArticle) : ""
                textFormat: Text.StyledText

                Accessible.role: Accessible.Heading
                Accessible.name: root.controller ? root.controller.prompt : ""
            }
        ]

        secondary: DesktopOptionGrid
        {
            id: optionGrid

            Layout.fillWidth: true

            cardModel: root.controller ? root.controller.options : []
            verbGroups: root.controller ? root.controller.optionVerbGroups : []
            stateColours: root.stateColours
            keyHints: ["1", "2", "3", "4", "5", "6", "7", "8", "9"]
            interactive: root.awaiting

            onOptionClicked: function(index) { root.controller.choose(index) }
        }

        controls: [
            DesktopButton
            {
                objectName: "exercise_dont_know"

                visible: root.awaiting
                Layout.preferredWidth: AppMetrics.actionButtonWidth
                large: true
                tone: "secondary"
                text: qsTr("Don't know")
                keyHint: "0"

                onClicked: root.dontKnow()
            },
            DesktopButton
            {
                objectName: "exercise_continue"

                Layout.preferredWidth: AppMetrics.actionButtonWidth
                large: true
                tone: "primary"
                text: qsTr("Continue")
                keyHint: qsTr("Enter")
                visible: !!root.controller && !root.controller.finished && root.controller.answered

                onClicked: root.advance()
            }
        ]
    }

    MouseArea
    {
        anchors.fill: parent
        enabled: correctAdvanceDelay.running

        onClicked: root.advance()
    }
}
