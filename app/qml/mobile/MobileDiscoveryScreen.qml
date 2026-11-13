import QtQuick
import QtQuick.Layouts

Item
{
    id: root

    required property var controller
    property var store: null

    signal done()
    signal wordTapped(string word, string sentence)
    signal editRequested(var card)

    readonly property bool roundOver: root.controller.roundOver
    readonly property bool answered: root.controller.answered
    readonly property bool awaiting: !root.roundOver && !root.answered
    readonly property bool beginnerPrompt: root.controller.promptKind === "beginner" && !root.answered
    readonly property bool showsSentence: root.controller.promptKind === "sentence" || root.controller.promptKind === "beginner"
        || root.controller.showsExample || root.answered
    readonly property var pickedTranslations: root.controller.translation.split(",")
        .map(Part => Part.trim().toLowerCase())
        .filter(Part => Part.length > 0)
    readonly property var newCards: root.controller.results.filter(Row => Row.learn)
    readonly property real resultTileWidth: Math.floor((root.width - AppMetrics.contentPadding * 2 - AppMetrics.gapSmall) / 2)

    property bool revealed: false
    property int wrongIndex: -1

    readonly property var borderColours:
    {
        const colours = []

        for (let index = 0; index < root.controller.options.length; ++index)
        {
            if (!root.answered)
            {
                colours.push(index === root.wrongIndex ? AppTheme.colourWrong : AppTheme.colourBorder)
            }
            else if (index === root.controller.correctOptionIndex)
            {
                colours.push(AppTheme.colourCorrect)
            }
            else
            {
                colours.push(index === root.controller.chosenOptionIndex ? AppTheme.colourWrong : AppTheme.colourBorder)
            }
        }

        return colours
    }

    readonly property var cellOpacities: root.controller.struckOptions
        .map((Struck, Index) => Struck && Index !== root.wrongIndex ? AppTheme.opacityDisabled : 1.0)

    function choose(index)
    {
        if (!root.awaiting || root.controller.struckOptions[index])
        {
            return
        }

        const correct = index === root.controller.correctOptionIndex

        optionGrid.armFeedback(correct)
        root.controller.choose(index)

        if (!correct && !root.answered)
        {
            root.wrongIndex = index
            wrongFlash.restart()
        }
    }

    function dontKnow()
    {
        if (!root.awaiting)
        {
            return
        }

        if (root.controller.lastClue)
        {
            optionGrid.armFeedback(false)
        }

        root.controller.askClue()
    }

    function pickTranslation(text)
    {
        const wanted = text.trim().toLowerCase()
        const kept = root.controller.translation.split(",")
            .map(Part => Part.trim())
            .filter(Part => Part.length > 0 && Part.toLowerCase() !== wanted)

        if (kept.length === root.pickedTranslations.length)
        {
            kept.push(text)
        }

        if (kept.length > 0)
        {
            root.controller.setTranslation(kept.join(", "))
        }
    }

    function commit(learn)
    {
        shell.swap(function() { root.controller.commit(learn) })
    }

    function clue()
    {
        if (root.answered)
        {
            if (root.controller.outcome === "known" && root.controller.promptKind !== "beginner")
            {
                root.commit(root.controller.learn)

                return
            }

            root.revealed = true

            return
        }

        root.dontKnow()
    }

    function leave()
    {
        root.controller.leave()
        root.done()
    }

    function endRound()
    {
        root.controller.endSession()
        root.done()
    }

    onAnsweredChanged:
    {
        wrongFlash.stop()
        root.wrongIndex = -1

        if (!root.answered)
        {
            root.revealed = false
        }
    }

    Timer
    {
        id: wrongFlash

        interval: AppMetrics.exerciseAdvanceDelayCorrect

        onTriggered: root.wrongIndex = -1
    }

    MobileExerciseShell
    {
        id: shell

        anchors.fill: parent
        visible: !root.roundOver

        exerciseName: root.controller.deckName.length > 0 ? qsTr("Discover · %1").arg(root.controller.deckName) : qsTr("Discover")
        store: root.store
        controller: root.controller
        progress: root.controller.exerciseProgress
        inSession: true
        showCompleteStage: false

        onExitRequested: root.leave()
        onRestartRequested: root.controller.restartStage()
        onEndSessionRequested: root.endRound()

        actions: [
            MobileSessionButton
            {
                objectName: "discovery_clue"

                visible: !root.revealed
                label: root.answered ? qsTr("Continue") : root.controller.lastClue ? qsTr("Show me") : qsTr("Not sure")

                onClicked: root.clue()
            },
            RowLayout
            {
                Layout.fillWidth: true
                visible: root.revealed
                spacing: AppMetrics.gapSmall

                MobileSessionButton
                {
                    objectName: "discovery_know"

                    label: qsTr("Know")
                    markColour: AppTheme.colourCorrect
                    filled: !root.controller.learn

                    onClicked: root.commit(false)
                }

                MobileSessionButton
                {
                    objectName: "discovery_learn"

                    label: qsTr("Learn")
                    markColour: AppTheme.colourAccent
                    filled: root.controller.learn

                    onClicked: root.commit(true)
                }
            }
        ]
        swipeEnabled: root.revealed
        swipeHints: true
        swipeLeftColour: AppTheme.colourCorrect
        swipeRightColour: AppTheme.colourAccent

        onSwiped: function(direction)
        {
            const learn = direction > 0

            shell.swipeOut(function() { root.controller.commit(learn) }, direction)
        }

        MobileContentArea
        {
            id: contentArea

            showPrimary: !root.revealed
            primaryShare: 60
            primaryCentred: true
            primaryFitsContent: true
            secondaryFillsArea: root.revealed
            swipeColour: shell.swipeFrameColour
            swipeStrength: shell.swipeHintStrength
            posText: root.controller.partOfSpeech === "noun" && root.controller.gender.length > 0
                ? qsTr("%1 · %2").arg(root.controller.partOfSpeech).arg(root.controller.gender)
                : root.controller.partOfSpeech
            posVisible: root.controller.partOfSpeech.length > 0
            counterText: qsTr("%1 / %2").arg(root.controller.askedCount).arg(root.controller.questionCount)

            primaryContent: [
                MobilePromptText
                {
                    objectName: "discovery_word"

                    text: root.controller.displayWord
                    gender: root.controller.headwordGender
                    maximumSize: AppMetrics.sessionAnswerSize

                    TapHandler { onTapped: root.controller.speakWord() }
                },
                MobileSentenceText
                {
                    objectName: "discovery_sentence"

                    Layout.fillWidth: true
                    visible: root.showsSentence && !root.revealed
                    sentence: root.beginnerPrompt ? root.controller.beginnerSentence : root.controller.example
                    gapStart: root.beginnerPrompt ? root.controller.beginnerSpanStart : root.controller.exampleSpanStart
                    gapLength: root.beginnerPrompt ? root.controller.beginnerSpanLength : root.controller.exampleSpanLength
                    tapEnabled: !root.beginnerPrompt
                    holdEnabled: false
                    horizontalAlignment: Text.AlignHCenter

                    onWordTapped: function(word, sentence) { root.wordTapped(word, sentence) }
                },
                Text
                {
                    objectName: "discovery_sentence_translation"

                    Layout.fillWidth: true
                    visible: root.showsSentence && !root.revealed && root.controller.example.length > 0
                        && root.controller.exampleTranslation.length > 0
                    opacity: root.answered ? 1 : 0
                    text: root.controller.exampleTranslation
                    font.pixelSize: AppMetrics.fontBody
                    font.family: AppMetrics.fontFamily
                    color: AppTheme.colourMuted
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }
            ]

            secondaryContent: [
                MobileOptionGrid
                {
                    id: optionGrid

                    objectName: "discovery_options"

                    Layout.fillWidth: true
                    visible: !root.revealed
                    rowSpacing: AppMetrics.gapSmall
                    columnSpacing: AppMetrics.gapSmall

                    cardModel: root.controller.options
                    borderColours: root.borderColours
                    cellOpacities: root.cellOpacities
                    interactive: root.awaiting

                    onOptionClicked: function(index) { root.choose(index) }
                },
                Item
                {
                    visible: root.revealed
                    Layout.fillHeight: true
                },
                MobilePromptText
                {
                    objectName: "discovery_revealed_word"

                    visible: root.revealed
                    text: root.controller.displayWord
                    gender: root.controller.headwordGender
                    maximumSize: AppMetrics.sessionAnswerSize

                    TapHandler { onTapped: root.controller.speakWord() }
                },
                Rectangle
                {
                    Layout.fillWidth: true
                    visible: root.revealed
                    Layout.preferredHeight: AppMetrics.previewFieldHeight
                    radius: AppMetrics.radiusSmall
                    color: AppTheme.colourFaint
                    border.color: AppTheme.colourBorder
                    border.width: AppMetrics.borderWidth

                    MobileTextInput
                    {
                        id: translationField

                        objectName: "discovery_translation"

                        anchors.fill: parent
                        anchors.leftMargin: AppMetrics.gapCompact
                        anchors.rightMargin: AppMetrics.gapCompact
                        verticalAlignment: TextInput.AlignVCenter
                        horizontalAlignment: TextInput.AlignHCenter

                        font.pixelSize: AppMetrics.fontLabel
                        font.weight: Font.Medium
                        font.family: AppMetrics.fontFamily
                        color: AppTheme.colourText

                        text: root.controller.translation

                        onTextEdited: root.controller.setTranslation(translationField.text)
                    }
                },
                MobileTriageMeaning
                {
                    objectName: "discovery_meaning"

                    visible: root.revealed
                    chipRows: 2
                    animated: false
                    meaning: root.revealed ? root.controller.meaning : ({ gist: "", nativeDefinition: "", learnedStage: -1 })
                    translations: root.controller.translationOptions
                    picked: root.pickedTranslations
                    expanded: true
                    stageCount: root.store ? root.store.stageCount : 1

                    onTranslationPicked: function(text) { root.pickTranslation(text) }
                    onWordTapped: function(word, sentence) { root.wordTapped(word, sentence) }
                },
                Item
                {
                    visible: root.revealed
                    Layout.fillHeight: true
                }
            ]
        }
    }

    Rectangle
    {
        anchors.fill: parent
        visible: root.roundOver
        color: AppTheme.colourPanel

        ColumnLayout
        {
            anchors.fill: parent
            spacing: 0

            MobileHeader
            {
                title: qsTr("Discover")
                showBack: true

                onBackRequested: root.leave()
            }

            Flickable
            {
                id: resultArea

                objectName: "discovery_results"

                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: width
                contentHeight: resultColumn.y + resultColumn.implicitHeight + AppMetrics.contentPadding
                boundsBehavior: Flickable.StopAtBounds
                clip: true

                ColumnLayout
                {
                    id: resultColumn

                    x: AppMetrics.contentPadding
                    y: Math.max(AppMetrics.contentPadding, (resultArea.height - resultColumn.implicitHeight) * AppMetrics.pickTopFill)
                    width: resultArea.width - AppMetrics.contentPadding * 2
                    spacing: AppMetrics.gapTight

                    Text
                    {
                        objectName: "discovery_added_count"

                        Layout.fillWidth: true
                        text: qsTr("%1 / %2").arg(root.newCards.length).arg(root.controller.results.length)
                        font.pixelSize: AppMetrics.fontPickTitle
                        font.weight: Font.DemiBold
                        font.family: AppMetrics.fontFamily
                        color: AppTheme.colourText
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Text
                    {
                        Layout.fillWidth: true
                        Layout.bottomMargin: AppMetrics.gapLarge
                        text: root.newCards.length > 0 ? qsTr("words added to My Vocabulary")
                                                       : qsTr("You knew every word of this round")
                        font.pixelSize: AppMetrics.fontBody
                        font.family: AppMetrics.fontFamily
                        color: AppTheme.colourMuted
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }

                    DesktopCenteredFlow
                    {
                        Layout.fillWidth: true
                        visible: root.newCards.length > 0
                        spacing: AppMetrics.gapSmall

                        Accessible.role: Accessible.List
                        Accessible.name: qsTr("Words added in this round")

                        Repeater
                        {
                            model: root.newCards

                            delegate: Item
                            {
                                id: resultTile

                                required property var modelData

                                readonly property bool editable: !!resultTile.modelData.card

                                objectName: "discovery_result_" + resultTile.modelData.index

                                width: root.resultTileWidth
                                height: wordArea.height + translationBand.height

                                Accessible.role: Accessible.Button
                                Accessible.name: qsTr("Edit %1").arg(resultTile.modelData.word)
                                Accessible.description: resultTile.modelData.translation
                                Accessible.onPressAction: if (resultTile.editable) root.editRequested(resultTile.modelData.card)

                                TapHandler
                                {
                                    id: tileTap

                                    enabled: resultTile.editable

                                    onTapped: root.editRequested(resultTile.modelData.card)
                                }

                                Rectangle
                                {
                                    anchors.fill: parent
                                    radius: AppMetrics.radiusMedium
                                    color: AppTheme.colourPanel
                                }

                                Item
                                {
                                    id: wordArea

                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    height: wordLabel.implicitHeight + AppMetrics.gapLarge * 2

                                    Text
                                    {
                                        id: wordLabel

                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.verticalCenter: parent.verticalCenter
                                        anchors.leftMargin: AppMetrics.gapCompact
                                        anchors.rightMargin: AppMetrics.gapCompact
                                        text: resultTile.modelData.word
                                        font.pixelSize: AppMetrics.fontTitle
                                        font.weight: Font.Medium
                                        font.family: AppMetrics.fontFamily
                                        fontSizeMode: Text.HorizontalFit
                                        minimumPixelSize: AppMetrics.fontBody
                                        color: AppTheme.colourText
                                        horizontalAlignment: Text.AlignHCenter
                                        elide: Text.ElideRight
                                    }
                                }

                                Rectangle
                                {
                                    id: translationBand

                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.top: wordArea.bottom
                                    height: translationLabel.implicitHeight + AppMetrics.gapCompact * 2
                                    radius: AppMetrics.radiusMedium
                                    color: AppTheme.colourFaint

                                    Rectangle
                                    {
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.top: parent.top
                                        height: parent.radius
                                        color: parent.color
                                    }

                                    Text
                                    {
                                        id: translationLabel

                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.verticalCenter: parent.verticalCenter
                                        anchors.leftMargin: AppMetrics.gapCompact
                                        anchors.rightMargin: AppMetrics.gapCompact
                                        text: resultTile.modelData.translation
                                        font.pixelSize: AppMetrics.fontBody
                                        font.family: AppMetrics.fontFamily
                                        color: AppTheme.colourMuted
                                        horizontalAlignment: Text.AlignHCenter
                                        elide: Text.ElideRight
                                    }
                                }

                                Rectangle
                                {
                                    anchors.fill: parent
                                    radius: AppMetrics.radiusMedium
                                    color: "transparent"
                                    border.width: AppMetrics.borderWidth
                                    border.color: tileTap.pressed ? AppTheme.colourAccent : AppTheme.colourBorder
                                }
                            }
                        }
                    }
                }
            }

            RowLayout
            {
                Layout.fillWidth: true
                Layout.leftMargin: AppMetrics.contentPadding
                Layout.rightMargin: AppMetrics.contentPadding
                Layout.topMargin: AppMetrics.gapMedium
                Layout.bottomMargin: AppMetrics.gapLarge
                spacing: AppMetrics.gapSmall

                MobileWideButton
                {
                    objectName: "discovery_done"

                    label: qsTr("Exit")
                    filled: false

                    onClicked: root.leave()
                }

                MobileWideButton
                {
                    objectName: "discovery_again"

                    label: qsTr("Restart")

                    onClicked: root.controller.restartStage()
                }
            }
        }
    }
}
