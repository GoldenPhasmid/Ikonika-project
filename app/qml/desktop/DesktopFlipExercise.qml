import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item
{
    id: root

    property var controller: null
    property var store: null

    readonly property string kind: "flip"
    readonly property Item surface: card.surface
    readonly property bool editable: true

    readonly property string title: qsTr("Flip")
    readonly property bool voiceSupported: false
    readonly property bool voiceOn: false
    readonly property bool answered: !!root.controller && root.controller.answered

    signal wordClicked(string word, string sentence)

    function toggleVoice()
    {
    }

    function playVoice()
    {
        if (promptLabel.speakable)
        {
            promptLabel.speak()
        }
    }

    function reveal()
    {
        if (!root.controller.answered)
        {
            root.controller.advance()
        }
    }

    function answer(correct)
    {
        if (!root.answered)
        {
            return
        }

        card.swap(function()
        {
            if (correct)
            {
                root.controller.answerYes()

                return
            }

            root.controller.answerNo()
        })
    }

    function handleKey(event)
    {
        if (!root.controller)
        {
            return false
        }

        if (!root.answered && (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space || event.key === Qt.Key_Down))
        {
            root.reveal()

            return true
        }

        if (event.key === Qt.Key_Left || event.key === Qt.Key_1)
        {
            root.answer(false)

            return true
        }

        if (event.key === Qt.Key_Right || event.key === Qt.Key_2)
        {
            root.answer(true)

            return true
        }

        return false
    }

    DesktopExerciseCard
    {
        id: card

        anchors.fill: parent

        tags: card.wordTags(root.controller)
        counterText: root.controller ? qsTr("%1 / %2").arg(root.controller.currentSessionCount).arg(root.controller.maxSessionCount) : ""

        primary: DesktopPromptLabel
        {
            id: promptLabel

            text: root.controller ? promptLabel.withArticle(root.controller.prompt, root.controller.promptArticle || "") : ""
            textFormat: Text.StyledText
            speakable: !!(root.controller && root.controller.showsTargetLanguage)
            gender: root.controller ? root.controller.headwordGender : -1
            verbGroup: root.controller ? root.controller.headwordVerbGroup : ""

            onSpeakRequested: root.controller.speakPrompt()

            Accessible.role: Accessible.Heading
            Accessible.name: root.controller ? root.controller.prompt : ""
        }

        secondary: DesktopMeaningWell
        {
            revealed: root.answered
            controller: root.controller
            answer: root.answered ? root.controller.lastRevealedAnswer : ""

            onWordClicked: function(word, sentence) { root.wordClicked(word, sentence) }
        }

        overlay: MouseArea
        {
            anchors.fill: parent
            enabled: !root.answered
            cursorShape: Qt.PointingHandCursor

            onClicked: root.reveal()
        }

        controls: [
            DesktopButton
            {
                objectName: "exercise_reveal"

                visible: !root.answered
                Layout.preferredWidth: AppMetrics.actionButtonWidth
                large: true
                tone: "primary"
                text: qsTr("Reveal")
                keyHint: qsTr("Enter")

                onClicked: root.reveal()
            },
            DesktopButton
            {
                visible: root.answered
                Layout.preferredWidth: AppMetrics.actionButtonWidth
                large: true
                tone: "dangerOutline"
                iconName: "close"
                text: qsTr("I forgot")
                keyHint: qsTr("Left")

                onClicked: root.answer(false)
            },
            DesktopButton
            {
                objectName: "exercise_correct"

                visible: root.answered
                Layout.preferredWidth: AppMetrics.actionButtonWidth
                large: true
                tone: "primary"
                iconName: "check"
                text: qsTr("I knew it")
                keyHint: qsTr("Right")

                onClicked: root.answer(true)
            }
        ]
    }
}
