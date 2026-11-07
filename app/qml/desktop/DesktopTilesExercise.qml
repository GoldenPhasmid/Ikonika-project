import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item
{
    id: root

    property var controller: null
    property var store: null

    readonly property string kind: "tiles"
    readonly property Item surface: card.surface
    readonly property bool editable: false

    signal wordClicked(string word, string sentence)

    readonly property bool sessionStage: !!root.controller && root.controller.typingAllowed
    readonly property bool typing: root.sessionStage && root.controller.typing

    readonly property string title: root.typing
        ? qsTr("Write the sentence")
        : (root.controller && root.controller.buildsTranslation ? qsTr("Listen and translate") : qsTr("Word tiles"))
    readonly property bool voiceSupported: false
    readonly property bool voiceOn: false
    readonly property bool typingSupported: root.sessionStage
    readonly property bool typingOn: root.typing

    readonly property bool revealed: !!root.controller && !root.controller.finished && root.controller.answered
    readonly property bool awaiting: !!root.controller && !root.controller.finished && !root.controller.answered

    readonly property var tiles: root.controller && root.controller.tiles ? root.controller.tiles : []
    readonly property var placed: root.controller && root.controller.placed ? root.controller.placed : []

    function toggleVoice()
    {
    }

    function toggleTyping()
    {
        root.controller.typing = !root.controller.typing
        Qt.callLater(root.focusAnswer)
    }

    function focusAnswer()
    {
        if (!root.awaiting)
        {
            return
        }

        if (root.typing)
        {
            answerField.forceActiveFocus()

            return
        }

        tileInput.forceActiveFocus()
    }

    function advance()
    {
        if (root.revealed)
        {
            card.swap(function()
            {
                answerField.clear()
                tileInput.clear()
                root.controller.advance()
                Qt.callLater(root.focusAnswer)
            })
        }
    }

    function normalised(word)
    {
        return word.toLowerCase().replace(/[’]/g, "'").replace(/[.,!?;:«»"“”()…¡¿]/g, "").trim()
    }

    function folded(word)
    {
        return root.normalised(word).normalize("NFD").replace(/[\u0300-\u036f]/g, "")
    }

    function findTile(typed, matches)
    {
        const found = []

        for (let index = 0; index < root.tiles.length; ++index)
        {
            if (root.placed.indexOf(index) < 0 && matches(root.tiles[index], typed))
            {
                found.push(index)
            }
        }

        return found
    }

    function placeTyped()
    {
        const typed = tileInput.text

        if (root.normalised(typed).length === 0)
        {
            return false
        }

        if (root.controller.canSubmit)
        {
            typingChip.shake()

            return false
        }

        const rules = [
            function(tile, text) { return root.normalised(tile) === root.normalised(text) },
            function(tile, text) { return root.folded(tile) === root.folded(text) },
            function(tile, text) { return root.folded(tile).startsWith(root.folded(text)) }
        ]

        for (let rule = 0; rule < rules.length; ++rule)
        {
            const found = root.findTile(typed, rules[rule])

            if (found.length === 1 || (found.length > 1 && rule < rules.length - 1))
            {
                tileInput.clear()
                root.controller.placeTile(found[0])

                return true
            }
        }

        typingChip.shake()

        return false
    }

    function submitTiles()
    {
        if (root.normalised(tileInput.text).length > 0 && !root.placeTyped())
        {
            return
        }

        root.controller.submit()
    }

    function removeLastTile()
    {
        if (root.placed.length > 0)
        {
            root.controller.removeTile(root.placed[root.placed.length - 1])
        }
    }

    function submitOrAdvance()
    {
        if (root.revealed)
        {
            root.advance()

            return
        }

        if (!root.awaiting)
        {
            return
        }

        if (root.typing)
        {
            root.controller.submitTyped(answerField.text + answerField.preeditText)

            return
        }

        root.submitTiles()
    }

    function playVoice()
    {
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

            if (root.typing)
            {
                answerField.shake()
            }
        }
    }

    onControllerChanged: Qt.callLater(root.focusAnswer)
    onVisibleChanged: if (root.visible) Qt.callLater(root.focusAnswer)
    onTypingChanged: Qt.callLater(root.focusAnswer)

    DesktopSentenceFit
    {
        id: sentenceFit

        width: card.surface.width - AppMetrics.contentPadding * 2
        availableHeight: card.primaryHeight
        foreignText: root.controller && !root.controller.finished ? root.controller.answer : ""
        nativeText: root.revealed ? root.controller.lastTranslation : ""
    }

    DesktopExerciseCard
    {
        id: card

        anchors.fill: parent

        primarySpeaks: root.revealed
        onPrimaryClicked: root.playVoice()

        counterText: root.controller ? qsTr("%1 / %2").arg(root.controller.currentSessionCount).arg(root.controller.maxSessionCount) : ""
        masteryTarget: root.controller ? root.controller.masteryTarget : 0
        masteryFilled: root.controller ? root.controller.cardMasteryCount : 0

        primary: [
            DesktopSentenceText
            {
                maximumSize: sentenceFit.foreignSize
                visible: root.revealed
                sentence: root.controller ? root.controller.lastRevealedAnswer : ""

                onWordClicked: function(word, sentence) { root.wordClicked(word, sentence) }
            },
            DesktopLabel
            {
                Layout.fillWidth: true
                visible: root.revealed && text.length > 0
                text: root.controller ? root.controller.lastTranslation : ""
                font.pixelSize: sentenceFit.nativeSize
                color: AppTheme.colourSubtle
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            },
            DesktopPlayButton
            {
                visible: !root.revealed
                Layout.alignment: Qt.AlignHCenter
                playing: root.controller ? root.controller.speaking : false
                keyHint: qsTr("Ctrl+Space")

                onClicked: root.controller.replay()
            },
            DesktopChipArea
            {
                visible: !root.typing && !root.revealed
                Layout.fillWidth: true
                Layout.maximumHeight: Math.max(AppMetrics.buttonHeight,
                    card.primaryHeight - AppMetrics.waveformButtonHeight - AppMetrics.gapSmall * 2)
                Layout.topMargin: AppMetrics.gapSmall
                spacing: AppMetrics.gapSmall

                Accessible.role: Accessible.List
                Accessible.name: qsTr("Words")

                Repeater
                {
                    model: root.tiles

                    delegate: DesktopWordTile
                    {
                        required property string modelData
                        required property int index

                        text: modelData
                        used: root.placed.indexOf(index) >= 0
                        enabled: root.awaiting && !used

                        onClicked:
                        {
                            root.controller.placeTile(index)
                            root.focusAnswer()
                        }
                    }
                }
            }
        ]

        secondary: ColumnLayout
        {
            Layout.fillWidth: true
            spacing: AppMetrics.gapSmall

            DesktopAnswerField
            {
                id: answerField

                objectName: "exercise_answer"

                visible: root.typing
                readOnly: !root.awaiting
                marked: root.revealed
                correct: !!root.controller && root.controller.lastAnswerCorrect

            }

            Rectangle
            {
                visible: !root.typing
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(card.secondaryHeight, answerTiles.implicitHeight + AppMetrics.gapSmall * 2)

                radius: AppMetrics.radiusMedium
                color: AppTheme.colourWell
                border.width: root.revealed ? AppMetrics.focusRingWidth : AppMetrics.borderWidth
                border.color: !root.revealed ? AppTheme.colourBorder
                                             : (root.controller.lastAnswerCorrect ? AppTheme.colourCorrect : AppTheme.colourWrong)

                Accessible.role: Accessible.List
                Accessible.name: qsTr("Your sentence")

                MouseArea
                {
                    anchors.fill: parent

                    onClicked: root.focusAnswer()
                }

                DesktopChipArea
                {
                    id: answerTiles

                    anchors.fill: parent
                    anchors.margins: AppMetrics.gapSmall
                    spacing: AppMetrics.gapSmall
                    centred: false
                    followEnd: true

                    Repeater
                    {
                        model: root.placed

                        delegate: DesktopWordTile
                        {
                            required property int modelData

                            text: root.tiles[modelData] || ""
                            enabled: root.awaiting

                            onClicked: root.controller.removeTile(modelData)
                        }
                    }

                    Rectangle
                    {
                        id: typingChip

                        property real shakeOffset: 0

                        function shake()
                        {
                            chipShake.restart()
                        }

                        visible: root.awaiting
                        width: Math.max(tileInput.contentWidth + AppMetrics.gapCompact * 2, AppMetrics.buttonHeight)
                        height: AppMetrics.buttonHeight

                        radius: AppMetrics.radiusMedium
                        color: AppTheme.colourCard
                        border.width: AppMetrics.borderWidth
                        border.color: tileInput.activeFocus ? AppTheme.colourAccent : AppTheme.colourBorder

                        transform: Translate { x: typingChip.shakeOffset }

                        DesktopShakeAnimation
                        {
                            id: chipShake

                            target: typingChip
                        }

                        TextInput
                        {
                            id: tileInput

                            objectName: "tiles_input"

                            anchors.fill: parent
                            anchors.leftMargin: AppMetrics.gapCompact
                            anchors.rightMargin: AppMetrics.gapCompact

                            readOnly: !root.awaiting || root.typing
                            verticalAlignment: TextInput.AlignVCenter
                            horizontalAlignment: TextInput.AlignHCenter
                            font.family: AppMetrics.fontFamily
                            font.pixelSize: AppMetrics.fontLabel
                            font.weight: Font.Medium
                            color: AppTheme.colourText
                            selectionColor: AppTheme.colourAccent
                            inputMethodHints: Qt.ImhNoPredictiveText

                            Accessible.name: qsTr("Type a word")

                            Keys.onSpacePressed: function(event)
                            {
                                event.accepted = event.text === " " && !(event.modifiers & Qt.ControlModifier)

                                if (event.accepted)
                                {
                                    root.placeTyped()
                                }
                            }

                            Keys.onPressed: function(event)
                            {
                                if (event.key === Qt.Key_Backspace && tileInput.text.length === 0)
                                {
                                    root.removeLastTile()
                                    event.accepted = true
                                }
                            }
                        }
                    }
                }
            }

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
                keyHint: qsTr("Ctrl+D")

                onClicked: root.dontKnow()
            },
            DesktopButton
            {
                objectName: "exercise_continue"

                visible: root.revealed || root.awaiting
                Layout.preferredWidth: AppMetrics.actionButtonWidth
                large: true
                tone: "primary"
                text: root.revealed ? qsTr("Continue") : qsTr("Submit")
                keyHint: qsTr("Enter")
                enabled: root.revealed || (root.awaiting && (root.typing
                    ? (answerField.text + answerField.preeditText).trim().length > 0
                    : root.controller.canSubmit))

                onClicked: root.submitOrAdvance()
            }
        ]
    }
}
