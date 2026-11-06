import QtQuick
import QtQuick.Layouts

Item
{
    id: root

    required property var store

    signal completed()
    signal done()
    signal wordTapped(string word, string sentence)

    readonly property alias controller: controller
    readonly property var meanings: controller.meaningOptions
    readonly property bool hasMeanings: root.meanings.length > 0

    property string pickedGist: ""
    property string meaningTranslation: ""
    property string openGist: ""

    readonly property var pickedTranslations: root.meaningTranslation.split(",")
        .map(Part => Part.trim().toLowerCase())
        .filter(Part => Part.length > 0)

    readonly property int defaultOpenIndex: Math.max(0, root.meanings.findIndex(Meaning => Meaning.learnedStage < 0 && Meaning.translations.length > 0))
    readonly property int pickedOpenIndex: root.meanings.findIndex(Meaning => Meaning.gist === root.openGist)
    readonly property int openIndex: root.pickedOpenIndex >= 0 ? root.pickedOpenIndex : (root.hasMeanings ? root.defaultOpenIndex : -1)

    readonly property bool canLearn: controller.word.length > 0
                                     && (!root.hasMeanings || (root.pickedGist.length > 0 && root.meaningTranslation.trim().length > 0))

    function learn()
    {
        if (root.hasMeanings)
        {
            controller.learnMeaning(root.pickedGist, root.meaningTranslation)

            return
        }

        controller.markUnknown()
    }

    function pickTranslation(gist, text)
    {
        if (root.pickedGist !== gist)
        {
            root.pickedGist = gist
            root.meaningTranslation = text

            return
        }

        const wanted = text.trim().toLowerCase()
        const kept = root.meaningTranslation.split(",")
            .map(Part => Part.trim())
            .filter(Part => Part.length > 0 && Part.toLowerCase() !== wanted)

        if (kept.length === root.pickedTranslations.length)
        {
            kept.push(text)
        }

        root.meaningTranslation = kept.join(", ")

        if (kept.length === 0)
        {
            root.pickedGist = ""
        }
    }

    function translationRow(text)
    {
        const wanted = text.trim().toLowerCase()

        return controller.translationOptions.find(Option => Option.text.trim().toLowerCase() === wanted)
            || ({ text: text, relative: 0, hasProbability: false, learnedStage: -1 })
    }

    function expandMeaning(index)
    {
        root.openGist = root.meanings[index].gist

        Qt.callLater(root.revealMeaning, index)
    }

    function revealMeaning(index)
    {
        const item = meaningRepeater.itemAt(index)

        if (!item)
        {
            return
        }

        let top = meaningList.y + meaningRepeater.itemAt(0).y

        for (let previous = 0; previous < index; ++previous)
        {
            top += meaningRepeater.itemAt(previous).targetHeight + meaningList.spacing
        }

        contentArea.revealSecondary(top, top + item.targetHeight)
    }

    function resetWord()
    {
        contentArea.revealSecondary(0, 0)
        root.pickedGist = ""
        root.meaningTranslation = ""
        root.openGist = ""

        const first = root.meanings.findIndex(Meaning => Meaning.learnedStage < 0 && Meaning.translations.length > 0)

        if (first >= 0)
        {
            root.openGist = root.meanings[first].gist
            root.pickTranslation(root.meanings[first].gist, root.meanings[first].translations[0])
        }
    }

    function start(wantedCount)
    {
        return controller.start(wantedCount)
    }

    TriageController
    {
        id: controller

        onProgressChanged: if (controller.finished) Qt.callLater(root.completed)
        onCurrentChanged: Qt.callLater(root.resetWord)
    }

    Component.onCompleted: Qt.callLater(root.resetWord)

    MobileExerciseShell
    {
        id: shell

        anchors.fill: parent

        exerciseName: qsTr("New words")
        store: root.store
        progress: controller.targetCount > 0 ? controller.progressCount / controller.targetCount : 0

        showOptions: false

        onExitRequested: root.done()

        swipeEnabled: controller.word.length > 0
        onSwiped: function(direction)
        {
            if (direction > 0 && !root.canLearn)
            {
                return
            }

            shell.swipeOut(direction > 0 ? function() { root.learn() } : function() { controller.markKnown() },
                           direction)
        }

        MobileContentArea
        {
            id: contentArea

            primaryShare: 25
            primaryCentred: true
            primaryFitsContent: true
            posText: controller.cardMeta
            posVisible: controller.cardMeta.length > 0
            counterText: qsTr("%1 / %2").arg(controller.progressCount).arg(controller.targetCount)

            primaryContent: [
                MobilePromptText
                {
                    text: controller.displayWord
                    gender: controller.headwordGender
                    maximumSize: AppMetrics.sessionAnswerSize
                },
                Rectangle
                {
                    Layout.fillWidth: true
                    visible: !root.hasMeanings
                    Layout.preferredHeight: AppMetrics.previewFieldHeight
                    radius: AppMetrics.radiusSmall
                    color: AppTheme.colourFaint
                    border.color: AppTheme.colourBorder
                    border.width: AppMetrics.borderWidth

                    MobileTextInput
                    {
                        id: translationField

                        anchors.fill: parent
                        anchors.leftMargin: AppMetrics.gapCompact
                        anchors.rightMargin: AppMetrics.gapCompact
                        verticalAlignment: TextInput.AlignVCenter
                        horizontalAlignment: TextInput.AlignHCenter

                        font.pixelSize: AppMetrics.fontLabel
                        font.weight: Font.Medium
                        font.family: AppMetrics.fontFamily
                        color: AppTheme.colourText

                        text: controller.translation
                        onTextEdited: if (text !== controller.translation) controller.translation = text
                    }
                }
            ]

            secondaryContent: ColumnLayout
            {
                id: meaningList

                Layout.fillWidth: true
                spacing: AppMetrics.chipListSpacing

                Repeater
                {
                    model: root.hasMeanings ? [] : controller.translationOptions

                    delegate: MobileTranslationChip
                    {
                        required property int index
                        required property var modelData

                        text: modelData.text
                        relative: modelData.relative
                        hasProbability: modelData.hasProbability
                        learnedStage: modelData.learnedStage
                        inUse: modelData.selected

                        onPicked: controller.toggleTranslation(index)
                    }
                }

                Repeater
                {
                    id: meaningRepeater

                    model: root.meanings

                    delegate: MobileTriageMeaning
                    {
                        required property var modelData
                        required property int index

                        stageCount: root.store.stageCount
                        meaning: modelData
                        translations: modelData.translations.map(Text => root.translationRow(Text))
                        picked: root.pickedGist === modelData.gist ? root.pickedTranslations : []
                        expanded: index === root.openIndex

                        onExpandRequested: root.expandMeaning(index)
                        onWordTapped: function(word, sentence) { root.wordTapped(word, sentence) }
                        onTranslationPicked: function(text) { root.pickTranslation(modelData.gist, text) }
                    }
                }
            }
        }

        actions: RowLayout
        {
            Layout.fillWidth: true
            spacing: AppMetrics.gapSmall

            MobileWideButton
            {
                label: qsTr("Skip")
                leadingArrow: "left"
                filled: false
                onClicked: shell.swap(function() { controller.markKnown() })
            }

            MobileWideButton
            {
                label: qsTr("Learn it")
                trailingArrow: "right"
                interactive: root.canLearn
                onClicked: shell.swap(function() { root.learn() })
            }
        }
    }
}
