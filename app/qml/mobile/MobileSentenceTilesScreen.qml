import QtQuick
import QtQuick.Layouts

Item
{
    id: root

    required property var controller

    property var store: null

    property bool showComplete: true
    property bool inSession: false
    property string exerciseName: root.typing
        ? qsTr("Write the sentence")
        : (root.controller && root.controller.buildsTranslation ? qsTr("Listen and translate") : qsTr("Word tiles"))

    readonly property bool finished: root.controller ? root.controller.finished : false
    readonly property bool answered: root.controller ? root.controller.answered : false
    readonly property bool revealed: !root.finished && root.answered

    readonly property bool sessionStage: !!root.controller && root.controller.typingAllowed
    readonly property bool typing: root.sessionStage && root.controller.typing
    readonly property bool awaiting: !root.finished && !root.answered
    readonly property bool bankShown: !root.typing && !root.revealed

    readonly property var tiles: root.controller && root.controller.tiles ? root.controller.tiles : []
    readonly property var placed: root.controller && root.controller.placed ? root.controller.placed : []

    readonly property color answerColour: !root.revealed
        ? AppTheme.colourBorder
        : (root.controller.lastAnswerCorrect ? AppTheme.colourCorrect : AppTheme.colourWrong)

    signal done()
    signal endSessionRequested()
    signal restartRequested()
    signal completeRequested()
    signal wordHeld(string word)
    signal wordTapped(string word, string sentence)

    function advance()
    {
        shell.swap(function()
        {
            answerField.clear()
            tileInput.clear()
            root.controller.advance()
            root.focusInput()
        })
    }

    function focusInput()
    {
        if (!root.visible || !root.awaiting)
        {
            return
        }

        if (root.typing)
        {
            answerField.forceActiveFocus()
        }
        else
        {
            tileInput.forceActiveFocus()
        }

        Qt.inputMethod.show()
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
        const typed = tileInput.text + tileInput.preeditText

        if (root.normalised(typed).length === 0)
        {
            tileInput.clear()

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
        const typed = tileInput.text + tileInput.preeditText

        if (root.normalised(typed).length > 0 && !root.placeTyped())
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
            if (answerField.text.trim().length > 0)
            {
                root.controller.submitTyped(answerField.text)
            }

            return
        }

        root.submitTiles()
    }

    onAwaitingChanged: if (!root.awaiting) tileInput.focus = false
    onVisibleChanged: if (root.visible) Qt.callLater(root.focusInput)
    onTypingChanged: Qt.callLater(root.focusInput)
    Component.onCompleted: Qt.callLater(root.focusInput)

    Connections
    {
        target: root.controller

        function onAnswerGraded(correct)
        {
            if (!correct && root.typing)
            {
                answerField.shake()
            }
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

        showTypingOption: root.sessionStage
        typingOn: root.typing
        onTypingToggled: root.controller.typing = !root.controller.typing

        progress: root.controller ? root.controller.exerciseProgress : 0

        questionSideNames: []
        inSession: root.inSession

        onExitRequested: root.done()
        onEndSessionRequested: root.endSessionRequested()

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

        actions: MobileSessionButton
        {
            label: root.revealed ? qsTr("Continue") : qsTr("Submit")
            interactive: root.revealed || (root.awaiting && (root.typing
                ? answerField.text.trim().length > 0
                : !!root.controller && root.controller.canSubmit))

            onClicked: root.submitOrAdvance()
        }

        MobileContentArea
        {
            posVisible: false
            primaryFillsArea: true
            primaryFitsContent: true
            primaryShare: 40
            counterText: root.controller
                ? qsTr("%1 / %2").arg(root.controller.currentSessionCount).arg(root.controller.maxSessionCount)
                : ""

            primaryContent: [
                Item
                {
                    Layout.fillHeight: true
                },
                MobilePlayButton
                {
                    visible: !root.revealed
                    Layout.fillWidth: true
                    playing: root.controller ? root.controller.speaking : false

                    onClicked: root.controller.replay()
                },
                MobileSentenceText
                {
                    visible: root.revealed
                    sentence: root.controller ? root.controller.lastRevealedAnswer : ""
                    tapEnabled: true

                    onWordHeld: function(word) { root.wordHeld(word) }
                    onWordTapped: function(word, sentence) { root.wordTapped(word, sentence) }
                },
                Text
                {
                    visible: root.revealed
                    Layout.fillWidth: true
                    text: root.controller ? root.controller.lastTranslation : ""
                    font.pixelSize: AppMetrics.sessionOptionSize
                    font.weight: Font.Medium
                    font.family: AppMetrics.fontFamily
                    color: AppTheme.colourSubtle
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                },
                Item
                {
                    visible: !root.revealed
                    Layout.fillHeight: true
                },
                Item
                {
                    visible: root.revealed
                    Layout.preferredHeight: AppMetrics.gapLarge
                },
                MobileCenteredFlow
                {
                    visible: root.bankShown
                    Layout.fillWidth: true
                    spacing: AppMetrics.gapSmall

                    Repeater
                    {
                        model: root.tiles

                        delegate: MobileWordTile
                        {
                            required property string modelData
                            required property int index

                            text: modelData
                            interactive: !root.finished && !root.answered
                            used: root.placed.indexOf(index) >= 0

                            onClicked: root.controller.placeTile(index)
                            onHeld: root.wordHeld(text)
                        }
                    }
                }
            ]

            secondaryContent: [
                MobileAnswerField
                {
                    id: answerField

                    visible: root.typing
                    focus: root.visible && root.typing && root.awaiting
                    placeholderText: qsTr("Type the sentence...")
                    readOnly: !root.awaiting
                    marked: root.revealed
                    correct: !!root.controller && root.controller.lastAnswerCorrect

                    onAccepted: root.submitOrAdvance()
                },
                Rectangle
                {
                    visible: !root.typing
                    Layout.fillWidth: true
                    implicitHeight: Math.max(answerRow.implicitHeight, tileMeasure.implicitHeight)
                                    + AppMetrics.gapSmall * 2

                    radius: AppMetrics.radiusMedium
                    color: AppTheme.colourSunken
                    border.color: tileInput.activeFocus && root.awaiting ? AppTheme.colourAccent : root.answerColour
                    border.width: root.revealed ? AppMetrics.borderWidth * 2 : AppMetrics.borderWidth

                    Behavior on border.color { ColorAnimation { duration: AppMetrics.motionBase } }

                    MouseArea
                    {
                        anchors.fill: parent
                        enabled: root.awaiting

                        onClicked: tileInput.forceActiveFocus()
                    }

                    Flow
                    {
                        id: answerRow

                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: AppMetrics.gapSmall
                        spacing: AppMetrics.gapSmall

                        Repeater
                        {
                            model: root.placed

                            delegate: MobileWordTile
                            {
                                required property int modelData

                                text: root.tiles[modelData] || ""
                                interactive: !root.answered

                                onClicked: root.controller.removeTile(modelData)
                                onHeld: root.wordHeld(text)
                            }
                        }

                        Rectangle
                        {
                            id: typingChip

                            function shake()
                            {
                                chipShake.play()
                            }

                            visible: root.awaiting
                            width: Math.max(tileInput.contentWidth + AppMetrics.gapCompact * 2,
                                            tileMeasure.implicitHeight * 2)
                            height: tileMeasure.implicitHeight

                            radius: AppMetrics.radiusMedium
                            color: AppTheme.colourPanel
                            border.width: AppMetrics.borderWidth
                            border.color: tileInput.activeFocus ? AppTheme.colourAccent : AppTheme.colourBorder

                            transform: MobileShake { id: chipShake }

                            Text
                            {
                                anchors.centerIn: parent
                                visible: tileInput.text.length === 0 && tileInput.preeditText.length === 0
                                text: qsTr("type...")
                                font.pixelSize: AppMetrics.sessionOptionSize
                                font.weight: Font.Medium
                                font.family: AppMetrics.fontFamily
                                color: AppTheme.colourMuted
                            }

                            TextInput
                            {
                                id: tileInput

                                anchors.fill: parent
                                anchors.leftMargin: AppMetrics.gapCompact
                                anchors.rightMargin: AppMetrics.gapCompact

                                readOnly: !root.awaiting
                                verticalAlignment: TextInput.AlignVCenter
                                horizontalAlignment: TextInput.AlignHCenter
                                font.family: AppMetrics.fontFamily
                                font.pixelSize: AppMetrics.sessionOptionSize
                                font.weight: Font.Medium
                                color: AppTheme.colourText
                                selectionColor: AppTheme.colourAccent
                                inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase

                                Accessible.name: qsTr("Type a word")

                                onTextEdited:
                                {
                                    if (tileInput.text.endsWith(" "))
                                    {
                                        tileInput.text = tileInput.text.trim()
                                        root.placeTyped()
                                    }
                                }

                                onAccepted: root.submitTiles()

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
            ]
        }
    }

    MobileWordTile
    {
        id: tileMeasure

        visible: false
        text: "M"
    }

    MobileExerciseCompleteScreen
    {
        anchors.fill: parent
        visible: root.finished && root.showComplete

        exerciseName: root.exerciseName

        wordCount: root.controller ? root.controller.currentSessionCount : 0
        countLabel: qsTr("sentences practiced")
        elapsedSeconds: root.controller ? root.controller.elapsedSeconds : 0

        onDoneClicked: root.done()
    }
}
