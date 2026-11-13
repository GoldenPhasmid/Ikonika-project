import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item
{
    id: root

    property var controller: null
    property var store: null
    property bool sentenceText: false

    readonly property string kind: "flashcards"
    readonly property Item surface: card.surface
    readonly property bool editable: !root.sentenceText

    signal wordClicked(string word, string sentence, rect wordRect)

    readonly property string instruction: qsTr("Study cards")
    readonly property bool voiceSupported: false
    readonly property bool voiceOn: false

    function toggleVoice()
    {
    }

    function playVoice()
    {
        if (promptLabel.speakable)
        {
            promptLabel.speak()

            return
        }

        root.controller.replay()
    }

    function next()
    {
        card.swap(function() { root.controller.next() })
    }

    function previous()
    {
        if (root.controller.canGoPrevious)
        {
            card.swap(function() { root.controller.previous() })
        }
    }

    function handleKey(event)
    {
        if (!root.controller)
        {
            return false
        }

        if (event.key === Qt.Key_Right || event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
        {
            root.next()

            return true
        }

        if (event.key === Qt.Key_Left)
        {
            root.previous()

            return true
        }

        return false
    }

    DesktopSentenceFit
    {
        id: sentenceFit

        width: card.surface.width - AppMetrics.contentPadding * 2
        availableHeight: card.primaryHeight
        foreignText: root.sentenceText && root.controller ? root.controller.prompt : ""
    }

    DesktopExerciseCard
    {
        id: card

        anchors.fill: parent

        tags: card.wordTags(root.controller)
        counterText: root.controller ? qsTr("%1 / %2").arg(root.controller.currentSessionCount).arg(root.controller.maxSessionCount) : ""

        primary: [
            DesktopSentenceText
            {
                maximumSize: sentenceFit.foreignSize
                visible: root.sentenceText
                sentence: root.controller && root.sentenceText ? root.controller.prompt : ""

                onWordClicked: function(word, sentence, wordRect) { root.wordClicked(word, sentence, wordRect) }
            },
            DesktopPromptLabel
            {
                id: promptLabel

                visible: !root.sentenceText
                speakable: !root.sentenceText
                gender: root.controller ? root.controller.headwordGender : -1
                verbGroup: root.controller ? root.controller.headwordVerbGroup : ""

                onSpeakRequested: root.controller.replay()
                text: !root.controller ? ""
                    : (root.controller.wordMarkup.length > 0 ? root.controller.wordMarkup
                                                             : promptLabel.withArticle(root.controller.prompt, root.controller.promptArticle))
                textFormat: Text.StyledText

                Accessible.role: Accessible.Heading
                Accessible.name: root.controller ? root.controller.prompt : ""
            },
            DesktopLabel
            {
                objectName: "flashcard_phonetic"

                visible: !root.sentenceText
                text: root.controller && root.controller.phonetic.length > 0 ? root.controller.phonetic : " "
                font.pixelSize: AppMetrics.fontTitle
                color: AppTheme.colourMuted
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
            }
        ]

        secondary: DesktopMeaningWell
        {
            revealed: true
            controller: root.controller
            answer: root.controller ? root.controller.answer : ""

            onWordClicked: function(word, sentence, wordRect) { root.wordClicked(word, sentence, wordRect) }
        }

        overlay: MouseArea
        {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor

            onClicked: root.next()
        }

        controls: [
            DesktopButton
            {
                Layout.preferredWidth: AppMetrics.actionButtonWidth
                large: true
                tone: "secondary"
                iconName: "arrow_left"
                text: qsTr("Previous")
                keyHint: qsTr("Left")
                enabled: !!root.controller && root.controller.canGoPrevious

                onClicked: root.previous()
            },
            DesktopButton
            {
                objectName: "exercise_continue"

                Layout.preferredWidth: AppMetrics.actionButtonWidth
                large: true
                tone: "primary"
                text: root.controller && root.controller.canGoNext ? qsTr("Next") : qsTr("Finish")
                trailingIconName: root.controller && root.controller.canGoNext ? "arrow_right" : ""
                keyHint: qsTr("Enter")

                onClicked: root.next()
            }
        ]
    }
}
