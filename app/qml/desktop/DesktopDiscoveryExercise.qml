import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item
{
    id: root

    property var controller: null
    property var store: null

    signal wordClicked(string word, string sentence, rect wordRect)
    signal lookupRequested(string word, string sentence)
    signal doneRequested()
    signal editRequested(var card)

    readonly property string kind: "discovery"
    readonly property Item surface: card.surface
    readonly property bool editable: false
    readonly property bool voiceSupported: false
    readonly property bool voiceOn: false
    readonly property bool formsHidden: !root.revealed
    readonly property string instruction: root.controller && root.controller.deckName.length > 0
        ? qsTr("Discover · %1").arg(root.controller.deckName) : qsTr("Discover")

    readonly property bool roundOver: !root.controller || root.controller.roundOver
    readonly property bool answered: !!root.controller && root.controller.answered
    readonly property bool awaiting: !root.roundOver && !root.answered
    readonly property bool skipsReveal: root.answered && root.controller.outcome === "known" && root.promptKind !== "beginner"
    readonly property string promptKind: root.controller ? root.controller.promptKind : ""
    readonly property bool beginnerPrompt: root.promptKind === "beginner" && !root.answered
    readonly property bool showsSentence: root.promptKind === "sentence" || root.promptKind === "beginner"
        || (!!root.controller && root.controller.showsExample)
    readonly property var results: root.controller ? root.controller.results : []
    readonly property var addedResults: root.results.filter(Row => Row.learn)
    readonly property Item headword: root.revealed ? revealedWord.item : promptWord.item
    readonly property int resultColumns: Math.max(1, Math.floor((resultArea.width - AppMetrics.contentPadding * 2 + AppMetrics.gapMedium)
                                                                / (AppMetrics.resultTileMinimumWidth + AppMetrics.gapMedium)))
    readonly property real resultTileWidth: Math.floor((resultGrid.width - AppMetrics.gapMedium * (root.resultColumns - 1))
                                                       / root.resultColumns)
    readonly property string translation: root.controller ? root.controller.translation : ""

    property bool revealed: false
    property real contentOpacity: 1
    property int wrongIndex: -1

    readonly property var pickedTranslations: root.translation.split(",")
        .map(Part => Part.trim().toLowerCase())
        .filter(Part => Part.length > 0)

    readonly property real optionHeight: Math.min(AppMetrics.optionCellHeight,
        (card.secondaryHeight - optionGrid.rowSpacing - AppMetrics.gapMedium) / 3)

    readonly property var stateColours:
    {
        const colours = []

        if (!root.controller)
        {
            return colours
        }

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

    readonly property var cellOpacities: root.controller
        ? root.controller.struckOptions.map((Struck, Index) => Struck && Index !== root.wrongIndex ? AppTheme.opacityDisabled : 1.0) : []

    function pickTranslation(text)
    {
        const wanted = text.trim().toLowerCase()
        const kept = root.translation.split(",")
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

    function showReveal()
    {
        if (!root.answered || root.revealed || revealFade.running)
        {
            return
        }

        if (root.skipsReveal)
        {
            root.controller.commit(root.controller.learn)

            return
        }

        revealFade.restart()
    }

    function commit(learn)
    {
        if (root.revealed)
        {
            root.controller.commit(learn)
        }
    }

    function playVoice()
    {
        if (root.controller && root.headword)
        {
            root.headword.speak()
        }
    }

    function handleKey(event)
    {
        if (!root.controller)
        {
            return false
        }

        const commits = event.key === Qt.Key_Return || event.key === Qt.Key_Enter

        if (root.roundOver)
        {
            return false
        }

        const number = event.key - Qt.Key_1

        if (root.awaiting && number >= 0 && number < root.controller.options.length)
        {
            root.choose(number)

            return true
        }

        if (root.awaiting && (event.key === Qt.Key_5 || event.key === Qt.Key_0))
        {
            root.dontKnow()

            return true
        }

        if (!root.answered)
        {
            return false
        }

        if (!root.revealed)
        {
            if (commits || event.key === Qt.Key_Left || event.key === Qt.Key_Right)
            {
                root.showReveal()

                return true
            }

            return false
        }

        if (event.key === Qt.Key_Left || event.key === Qt.Key_Right)
        {
            root.commit(event.key === Qt.Key_Right)

            return true
        }

        if (commits)
        {
            root.commit(root.controller.learn)

            return true
        }

        return false
    }

    onAnsweredChanged:
    {
        wrongFlash.stop()
        root.wrongIndex = -1

        if (!root.answered)
        {
            revealFade.stop()
            root.revealed = false
            root.contentOpacity = 1
        }
    }

    Timer
    {
        id: wrongFlash

        interval: AppMetrics.exerciseAdvanceDelayCorrect

        onTriggered: root.wrongIndex = -1
    }

    SequentialAnimation
    {
        id: revealFade

        NumberAnimation
        {
            target: root
            property: "contentOpacity"
            to: 0
            duration: AppMetrics.transitionsEnabled ? AppMetrics.motionFast : 0
            easing.type: AppMetrics.motionEasing
        }

        ScriptAction { script: root.revealed = true }

        NumberAnimation
        {
            target: root
            property: "contentOpacity"
            to: 1
            duration: AppMetrics.transitionsEnabled ? AppMetrics.motionBase : 0
            easing.type: AppMetrics.motionEasing
        }
    }

    FontMetrics
    {
        id: sentenceMetrics

        font.pixelSize: AppMetrics.sessionTranslationSize
        font.weight: Font.Medium
    }

    Component
    {
        id: headwordComponent

        DesktopPromptLabel
        {
            id: headwordLabel

            opacity: root.contentOpacity
            speakable: true
            gender: root.controller ? root.controller.headwordGender : -1
            text: root.controller ? headwordLabel.withArticle(root.controller.displayWord, root.controller.article) : ""
            textFormat: Text.StyledText

            onSpeakRequested: root.controller.speakWord()

            Accessible.role: Accessible.Heading
            Accessible.name: root.controller ? root.controller.displayWord : ""
        }
    }

    DesktopExerciseCard
    {
        id: card

        objectName: "discovery_card"

        anchors.fill: parent
        visible: !root.roundOver
        showPrimary: !root.revealed
        fillSecondary: root.revealed

        tags: root.controller ? [root.controller.partOfSpeech, root.controller.partOfSpeech === "noun" ? root.controller.gender : ""] : []
        counterText: root.controller ? qsTr("%1 / %2").arg(root.controller.askedCount).arg(root.controller.questionCount) : ""
        primarySpeaks: !root.revealed

        onPrimaryClicked: root.playVoice()

        primary: [
            Loader
            {
                id: promptWord

                Layout.fillWidth: true
                Layout.fillHeight: true
                active: !root.revealed
                visible: active
                sourceComponent: headwordComponent
            },
            Item
            {
                objectName: "discovery_sentence_slot"

                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(sentenceMetrics.lineSpacing * 2, sentenceText.implicitHeight
                    + (sentenceTranslation.visible ? AppMetrics.gapSmall + sentenceTranslation.implicitHeight : 0))
                visible: !root.revealed
                opacity: root.contentOpacity

                DesktopSentenceText
                {
                    id: sentenceText

                    objectName: "discovery_sentence"

                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    opacity: root.showsSentence || root.answered ? 1 : 0
                    sentence: !root.controller ? ""
                        : root.beginnerPrompt ? root.controller.beginnerSentence : root.controller.example
                    gapStart: !root.controller ? -1
                        : root.beginnerPrompt ? root.controller.beginnerSpanStart : root.controller.exampleSpanStart
                    gapLength: !root.controller ? 0
                        : root.beginnerPrompt ? root.controller.beginnerSpanLength : root.controller.exampleSpanLength
                    interactive: sentenceText.opacity > 0 && !root.beginnerPrompt
                    verticalAlignment: Text.AlignTop

                    onWordClicked: function(word, sentence, wordRect) { root.wordClicked(word, sentence, wordRect) }
                }

                DesktopLabel
                {
                    id: sentenceTranslation

                    objectName: "discovery_sentence_translation"

                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: sentenceText.bottom
                    anchors.topMargin: AppMetrics.gapSmall
                    visible: root.answered && !!root.controller && root.controller.example.length > 0
                        && root.controller.exampleTranslation.length > 0
                    text: root.controller ? root.controller.exampleTranslation : ""
                    font.pixelSize: AppMetrics.sessionDefinitionSize
                    color: AppTheme.colourMuted
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }
            }
        ]

        secondary: [
            DesktopOptionGrid
            {
                id: optionGrid

                objectName: "discovery_options"

                Layout.fillWidth: true
                visible: !root.revealed
                opacity: root.contentOpacity

                cardModel: root.controller ? root.controller.options : []
                stateColours: root.stateColours
                cellOpacities: root.cellOpacities
                keyHints: ["1", "2", "3", "4"]
                interactive: root.awaiting
                cellHeight: root.optionHeight

                onOptionClicked: function(index) { root.choose(index) }
            },
            DesktopOptionGrid
            {
                objectName: "discovery_clue"

                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: false
                Layout.preferredWidth: (optionGrid.width - optionGrid.columnSpacing) / 2
                Layout.maximumWidth: Layout.preferredWidth
                visible: !root.revealed
                opacity: root.contentOpacity

                columns: 1
                cardModel: [root.answered ? qsTr("Continue")
                    : root.controller && root.controller.lastClue ? qsTr("Show me") : qsTr("Not sure")]
                filledCells: [true]
                keyHints: [root.answered ? qsTr("Enter") : "5"]
                interactive: root.awaiting || root.answered
                cellHeight: root.optionHeight

                onOptionClicked: root.answered ? root.showReveal() : root.dontKnow()
            },
            Item
            {
                Layout.fillHeight: true
                visible: root.revealed
            },
            Loader
            {
                id: revealedWord

                Layout.fillWidth: true
                Layout.preferredHeight: revealedWord.item ? revealedWord.item.implicitHeight : 0
                active: root.revealed
                visible: active
                sourceComponent: headwordComponent
            },
            DesktopTextField
            {
                id: translationField

                objectName: "discovery_translation"

                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: parent ? (parent.width - optionGrid.columnSpacing) / 2 : 0
                visible: root.revealed
                opacity: root.contentOpacity
                large: true
                horizontalAlignment: TextInput.AlignHCenter
                text: root.translation

                onTextEdited: root.controller.setTranslation(translationField.text)
            },
            DesktopTriageMeaning
            {
                objectName: "discovery_meaning"

                Layout.fillWidth: true
                Layout.topMargin: AppMetrics.focusRingWidth
                Layout.bottomMargin: AppMetrics.focusRingWidth
                visible: root.revealed
                opacity: root.contentOpacity
                meaning: root.controller ? root.controller.meaning : ({ gist: "", nativeDefinition: "", learnedStage: -1 })
                translations: root.controller ? root.controller.translationOptions : []
                picked: root.pickedTranslations
                expanded: true
                chipRows: 2
                stageCount: root.store ? root.store.stageCount : 1

                onTranslationPicked: function(text) { root.pickTranslation(text) }
                onWordClicked: function(word, sentence) { root.lookupRequested(word, sentence) }
            },
            Item
            {
                Layout.fillHeight: true
                visible: root.revealed
            },
            RowLayout
            {
                Layout.alignment: Qt.AlignHCenter
                visible: root.revealed
                opacity: root.contentOpacity
                spacing: AppMetrics.gapMedium

                DesktopButton
                {
                    objectName: "discovery_know"

                    Layout.preferredWidth: AppMetrics.actionButtonWidth
                    large: true
                    tone: !!root.controller && !root.controller.learn ? "primary" : "secondary"
                    text: qsTr("Know")
                    keyHint: !!root.controller && !root.controller.learn ? qsTr("Enter") : qsTr("Left")
                    focusPolicy: Qt.NoFocus

                    onClicked: root.commit(false)
                }

                DesktopButton
                {
                    objectName: "discovery_learn"

                    Layout.preferredWidth: AppMetrics.actionButtonWidth
                    large: true
                    tone: !!root.controller && root.controller.learn ? "primary" : "secondary"
                    text: qsTr("Learn")
                    keyHint: !!root.controller && root.controller.learn ? qsTr("Enter") : qsTr("Right")
                    focusPolicy: Qt.NoFocus

                    onClicked: root.commit(true)
                }
            }
        ]
    }

    Flickable
    {
        id: resultArea

        objectName: "discovery_results"

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: resultBar.top
        visible: root.roundOver
        contentWidth: width
        contentHeight: resultColumn.y + resultColumn.implicitHeight + AppMetrics.contentPadding
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        ScrollBar.vertical: DesktopScrollBar {}

        ColumnLayout
        {
            id: resultColumn

            x: (resultArea.width - width) / 2
            y: Math.max(AppMetrics.contentPadding, (resultArea.height - resultColumn.implicitHeight) * AppMetrics.pickTopFill)
            width: Math.min(AppMetrics.contentWidthWide, resultArea.width - AppMetrics.contentPadding * 2)
            spacing: AppMetrics.gapTight

            DesktopLabel
            {
                objectName: "discovery_added_count"

                Layout.fillWidth: true
                text: qsTr("%1 / %2").arg(root.addedResults.length).arg(root.results.length)
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: AppMetrics.fontPickTitle
                font.weight: Font.Medium
            }

            DesktopLabel
            {
                Layout.fillWidth: true
                Layout.bottomMargin: AppMetrics.contentPadding
                text: root.addedResults.length > 0 ? qsTr("words added to My Vocabulary")
                                                   : qsTr("You knew every word of this round")
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: AppMetrics.fontSubtitle
                color: AppTheme.colourSubtle
            }

            DesktopCenteredFlow
            {
                id: resultGrid

                Layout.fillWidth: true
                visible: root.addedResults.length > 0
                spacing: AppMetrics.gapMedium

                Accessible.role: Accessible.List
                Accessible.name: qsTr("Words added in this round")

                Repeater
                {
                    model: root.addedResults

                    delegate: Item
                    {
                        id: resultTile

                        required property var modelData

                        readonly property bool editable: !!resultTile.modelData.card
                        readonly property bool hovered: tileHover.hovered && resultTile.editable

                        function edit()
                        {
                            if (resultTile.editable)
                            {
                                root.editRequested(resultTile.modelData.card)
                            }
                        }

                        objectName: "discovery_result_" + resultTile.modelData.index

                        width: root.resultTileWidth
                        height: wordArea.height + translationBand.height
                        activeFocusOnTab: resultTile.editable

                        Accessible.role: Accessible.Button
                        Accessible.name: qsTr("Edit %1").arg(resultTile.modelData.word)
                        Accessible.description: resultTile.modelData.translation
                        Accessible.onPressAction: resultTile.edit()

                        Keys.onReturnPressed: resultTile.edit()
                        Keys.onEnterPressed: resultTile.edit()
                        Keys.onSpacePressed: resultTile.edit()

                        HoverHandler
                        {
                            id: tileHover

                            cursorShape: resultTile.editable ? Qt.PointingHandCursor : Qt.ArrowCursor
                        }

                        TapHandler
                        {
                            onTapped: resultTile.edit()
                        }

                        Rectangle
                        {
                            anchors.fill: parent
                            radius: AppMetrics.radiusLarge
                            color: AppTheme.colourCard
                        }

                        Item
                        {
                            id: wordArea

                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            height: wordLabel.implicitHeight + AppMetrics.gapLarge * 2

                            DesktopLabel
                            {
                                id: wordLabel

                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.leftMargin: AppMetrics.cardPadding
                                anchors.rightMargin: AppMetrics.cardPadding
                                text: resultTile.modelData.word
                                font.pixelSize: AppMetrics.fontSubtitle
                                font.weight: Font.Medium
                                fontSizeMode: Text.HorizontalFit
                                minimumPixelSize: AppMetrics.fontBody
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
                            radius: AppMetrics.radiusLarge
                            color: AppTheme.colourSunken

                            Rectangle
                            {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                height: parent.radius
                                color: parent.color
                            }

                            DesktopLabel
                            {
                                id: translationLabel

                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.leftMargin: AppMetrics.cardPadding
                                anchors.rightMargin: AppMetrics.cardPadding
                                text: resultTile.modelData.translation
                                color: AppTheme.colourSubtle
                                horizontalAlignment: Text.AlignHCenter
                                elide: Text.ElideRight
                            }
                        }

                        Rectangle
                        {
                            anchors.fill: parent
                            radius: AppMetrics.radiusLarge
                            color: "transparent"
                            border.width: AppMetrics.borderWidth
                            border.color: resultTile.hovered ? AppTheme.colourOutline : AppTheme.colourBorder

                            Behavior on border.color { ColorAnimation { duration: AppMetrics.motionFast } }
                        }

                        DesktopIcon
                        {
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: AppMetrics.gapMedium
                            width: AppMetrics.iconSize
                            height: AppMetrics.iconSize
                            name: "edit"
                            color: AppTheme.colourSubtle
                            opacity: resultTile.hovered || resultTile.activeFocus ? 1 : 0

                            Behavior on opacity { NumberAnimation { duration: AppMetrics.motionFast } }
                        }

                        DesktopFocusRing
                        {
                            visible: resultTile.activeFocus
                            baseRadius: AppMetrics.radiusLarge
                        }
                    }
                }
            }
        }
    }

    DesktopActionBar
    {
        id: resultBar

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        visible: root.roundOver
        height: visible ? implicitHeight : 0
        color: AppTheme.colourPage

        DesktopButton
        {
            objectName: "discovery_again"

            Layout.preferredWidth: AppMetrics.actionButtonWidth
            large: true
            tone: "primary"
            text: qsTr("Restart")

            onClicked: root.controller.restartStage()
        }

        DesktopButton
        {
            objectName: "discovery_done"

            Layout.preferredWidth: AppMetrics.actionButtonWidth
            large: true
            text: qsTr("Exit")

            onClicked: root.doneRequested()
        }
    }
}
