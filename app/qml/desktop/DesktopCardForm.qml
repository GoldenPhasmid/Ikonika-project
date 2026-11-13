import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ColumnLayout
{
    id: root

    property var controller: null
    property bool expanded: false

    property string editingCardId: ""
    property int editingStage: -1
    property int editingGender: -1
    property string editingPartOfSpeech: ""
    property string adoptedCardId: ""
    property string returnCardId: ""
    property int returnStage: -1
    property int returnGender: -1
    property string returnPartOfSpeech: ""
    property string returnHint: ""
    property bool readingChosen: false
    property var suggestions: []
    property string meaningGist: ""

    property string savedWord: ""
    property string savedTranslation: ""
    property int savedGender: -1
    property string savedPartOfSpeech: ""
    property string savedGist: ""
    property string savedHint: ""

    readonly property alias dictionary: dictionary

    signal accepted()
    signal wordClicked(string word, string sentence)

    readonly property bool editing: root.editingCardId.length > 0
    readonly property var editingCard: root.editing
        ? (dictionary.cards.find(Card => Card.cardId === root.editingCardId) || null) : null
    readonly property var meanings: (dictionary.entry.headword || "").toLowerCase() === root.enteredWord.toLowerCase()
        && (dictionary.entry.partOfSpeech || "") === root.editingPartOfSpeech
        ? (dictionary.entry.meanings || []) : []
    readonly property var meaning: root.meanings.find(Meaning => Meaning.gist === root.meaningGist) || ({})
    readonly property bool hasMeaning: !!root.meaning.gist
    readonly property string committedGist: root.hasMeaning || (root.editing && root.meanings.length === 0) ? root.meaningGist : ""
    readonly property var meaningOptions: [""].concat(root.meanings.map(Meaning => Meaning.gist))
    readonly property string enteredWord: (wordField.text + wordField.preeditText).trim()
    readonly property string enteredTranslation: (translationField.text + translationField.preeditText).trim()
    readonly property bool canCommit: root.enteredWord.length > 0 && root.enteredTranslation.length > 0
    readonly property bool typing: wordField.activeFocus || translationField.activeFocus || hintField.activeFocus

    readonly property bool changed: root.enteredWord !== root.savedWord
        || root.enteredTranslation !== root.savedTranslation
        || root.editingGender !== root.savedGender
        || root.editingPartOfSpeech !== root.savedPartOfSpeech
        || root.committedGist !== root.savedGist
        || hintField.text.trim() !== root.savedHint

    readonly property var pickedTranslations: root.enteredTranslation.split(",")
        .map(Part => Part.trim().toLowerCase())
        .filter(Part => Part.length > 0)

    readonly property var genderOptions: (dictionary.targetLanguageCode === "DE"
        ? [{ label: qsTr("Neuter"), value: 3 }] : []).concat([
        { label: qsTr("Masculine"), value: 0 },
        { label: qsTr("Feminine"), value: 1 },
        { label: qsTr("Either (m/f)"), value: 2 },
        { label: qsTr("None (no article)"), value: -1 }
    ])

    Layout.fillWidth: true
    spacing: AppMetrics.gapMedium

    function openWith(word, translation, partOfSpeech, gist, hint)
    {
        root.editingCardId = ""
        root.editingStage = -1
        root.editingGender = -1
        root.editingPartOfSpeech = ""
        root.readingChosen = false
        root.adoptedCardId = ""
        root.meaningGist = ""

        wordField.text = word
        translationField.text = translation
        hintField.text = hint || ""
        root.suggestions = []
        dictionary.openEntry("", "")

        if (word.length > 0)
        {
            dictionary.openEntry(word, partOfSpeech || "")
            root.adoptDictionaryReading(partOfSpeech)
            root.meaningGist = gist || root.findMeaningFor(translation)
            root.followMeaningCard()

            if (translationField.text.length === 0 && root.hasMeaning)
            {
                translationField.text = root.meaning.translations[0] || ""
            }

            root.rebuildSuggestions()
        }

        root.markSaved()
    }

    function openForEdit(cardId, word, translation, gender, partOfSpeech, box)
    {
        root.openWith(word, translation, partOfSpeech)

        root.editingCardId = cardId
        root.editingStage = box
        root.editingGender = gender
        root.editingPartOfSpeech = partOfSpeech
        root.adoptedCardId = ""
        translationField.text = translation
        root.meaningGist = dictionary.cardMeaningGist(cardId)
        hintField.text = dictionary.cardHint(cardId)
        root.rebuildSuggestions()
        root.markSaved()
    }

    function markSaved()
    {
        root.savedWord = root.enteredWord
        root.savedTranslation = root.enteredTranslation
        root.savedGender = root.editingGender
        root.savedPartOfSpeech = root.editingPartOfSpeech
        root.savedGist = root.committedGist
        root.savedHint = hintField.text.trim()
    }

    function focusField()
    {
        const target = root.enteredWord.length > 0 ? translationField : wordField

        target.forceActiveFocus()
        target.selectAll()
    }

    function adoptDictionaryReading(fallbackPartOfSpeech)
    {
        const known = (dictionary.entry.headword || "").toLowerCase() === root.enteredWord.toLowerCase()

        root.editingPartOfSpeech = known ? (dictionary.entry.partOfSpeech || "") : (fallbackPartOfSpeech || "")
        root.editingGender = root.dictionaryGender()
    }

    function dictionaryGender()
    {
        if (root.editingPartOfSpeech !== "noun")
        {
            return -1
        }

        const gendered = root.meanings.find(Meaning => Meaning.gender >= 0)

        return gendered ? gendered.gender : -1
    }

    function openWordEntry()
    {
        dictionary.openEntry(root.enteredWord, root.editing ? root.editingPartOfSpeech : "")

        if (!root.editing)
        {
            root.adoptDictionaryReading("")
        }

        root.rebuildSuggestions()
    }

    function followMeaningCard()
    {
        const chosen = root.meaning
        const cardId = chosen.cardId || ""

        if (cardId.length > 0)
        {
            if (cardId === root.editingCardId)
            {
                return
            }

            if (root.adoptedCardId.length === 0)
            {
                root.returnCardId = root.editingCardId
                root.returnStage = root.editingStage
                root.returnGender = root.editingGender
                root.returnPartOfSpeech = root.editingPartOfSpeech
                root.returnHint = hintField.text
            }

            console.info("Card form edits the card of meaning", chosen.gist, cardId)

            root.adoptedCardId = cardId
            root.editingPartOfSpeech = dictionary.entry.partOfSpeech || ""
            root.editingStage = chosen.learnedStage
            root.editingGender = chosen.cardGender
            root.editingCardId = cardId
            hintField.text = root.returnHint || dictionary.cardHint(cardId)
            translationField.text = chosen.cardTranslation

            return
        }

        if (root.adoptedCardId.length === 0)
        {
            return
        }

        root.adoptedCardId = ""
        root.editingPartOfSpeech = root.returnPartOfSpeech
        root.editingStage = root.returnStage
        root.editingGender = root.returnGender
        root.editingCardId = root.returnCardId
        hintField.text = root.returnHint
    }

    function findMeaningFor(translation)
    {
        const wanted = (translation || "").trim().toLowerCase()

        if (wanted.length === 0)
        {
            return root.meanings.length > 0 ? root.meanings[0].gist : ""
        }

        const found = root.meanings.find(Meaning => Meaning.translations.some(Text => Text.trim().toLowerCase() === wanted))

        return found ? found.gist : ""
    }

    function chooseMeaning(gist)
    {
        root.meaningGist = gist
        root.followMeaningCard()

        if (!root.hasMeaning || root.meaning.cardId)
        {
            root.rebuildSuggestions()

            return
        }

        if (root.editingPartOfSpeech === "noun" && root.meaning.gender >= 0)
        {
            root.editingGender = root.meaning.gender
        }

        const allowed = root.meaning.translations.map(Text => Text.trim().toLowerCase())
        const kept = root.enteredTranslation.split(",")
            .map(Part => Part.trim())
            .filter(Part => allowed.indexOf(Part.toLowerCase()) >= 0)

        translationField.text = kept.length > 0 ? kept.join(", ") : (root.meaning.translations[0] || "")
        root.rebuildSuggestions()
    }

    function hasTranslation(text)
    {
        return root.pickedTranslations.indexOf(text.trim().toLowerCase()) >= 0
    }

    function toggleTranslation(text)
    {
        const wanted = text.trim().toLowerCase()
        const entered = root.enteredTranslation

        if (!root.hasTranslation(text))
        {
            translationField.text = entered.length > 0 ? entered + ", " + text : text

            return
        }

        translationField.text = entered.split(",")
            .filter(Part => Part.trim().toLowerCase() !== wanted)
            .map(Part => Part.trim())
            .join(", ")
    }

    function rebuildSuggestions()
    {
        const result = []

        const meaningTranslations = root.hasMeaning
            ? root.meaning.translations.map(Text => Text.trim().toLowerCase()) : []

        for (const translation of dictionary.allTranslations)
        {
            if (root.hasMeaning && meaningTranslations.indexOf(translation.text.trim().toLowerCase()) < 0)
            {
                continue
            }

            result.push({
                text: translation.text,
                relative: translation.relative,
                hasProbability: translation.hasProbability,
                learnedStage: root.controller
                    ? root.controller.findWordTranslationStage(root.enteredWord, translation.text)
                    : translation.learnedStage,
            })
        }

        root.suggestions = result
    }

    function commit()
    {
        if (!root.canCommit)
        {
            return false
        }

        const meaningGender = root.hasMeaning ? root.meaning.gender : -1

        return root.editing
            ? root.controller.updateCard(root.editingCardId, root.enteredWord, root.enteredTranslation,
                                         root.editingGender, root.editingPartOfSpeech, root.committedGist, hintField.text.trim())
            : root.controller.addCard(root.enteredWord, root.enteredTranslation, root.editingPartOfSpeech,
                                      root.editingGender, root.readingChosen || meaningGender >= 0, root.committedGist, hintField.text.trim())
    }

    DictionaryController
    {
        id: dictionary
    }

    RowLayout
    {
        Layout.fillWidth: true
        spacing: AppMetrics.gapMedium

        DesktopFormField
        {
            Layout.preferredWidth: 1
            label: qsTr("Word")

            DesktopTextField
            {
                id: wordField

                objectName: "word_dialog_word"

                Layout.fillWidth: true
                placeholderText: qsTr("Word")
                maximumLength: AppMetrics.maximumCardFieldLength

                onAccepted:
                {
                    root.openWordEntry()
                    translationField.forceActiveFocus()
                }

                onEditingFinished:
                {
                    if ((dictionary.entry.headword || "") !== root.enteredWord)
                    {
                        root.openWordEntry()
                    }
                }
            }
        }

        DesktopFormField
        {
            Layout.preferredWidth: 1
            label: qsTr("Translation")

            DesktopTextField
            {
                id: translationField

                objectName: "word_dialog_translation"

                Layout.fillWidth: true
                placeholderText: qsTr("Translation")
                maximumLength: AppMetrics.maximumCardFieldLength

                onAccepted: root.accepted()
            }
        }
    }

    RowLayout
    {
        Layout.fillWidth: true
        spacing: AppMetrics.gapMedium

        DesktopFormField
        {
            Layout.preferredWidth: 1
            Layout.alignment: Qt.AlignTop
            label: qsTr("Part of speech")

            DesktopSelect
            {
                objectName: "word_dialog_part_of_speech"

                heading: qsTr("Part of speech")
                options: dictionary.partsOfSpeech.map(Option => Option.label)
                currentIndex: dictionary.partsOfSpeech.findIndex(Option => Option.value === root.editingPartOfSpeech)

                onChosen: function(index)
                {
                    root.editingPartOfSpeech = dictionary.partsOfSpeech[index].value
                    root.readingChosen = true

                    dictionary.openEntry(root.enteredWord, root.editingPartOfSpeech)

                    if (root.editingPartOfSpeech !== "noun" || root.editingGender < 0)
                    {
                        root.editingGender = root.dictionaryGender()
                    }

                    if (!root.hasMeaning)
                    {
                        root.meaningGist = ""
                    }

                    root.rebuildSuggestions()
                }
            }
        }

        DesktopFormField
        {
            Layout.preferredWidth: 1
            Layout.alignment: Qt.AlignTop
            label: qsTr("Gender")
            opacity: root.editingPartOfSpeech === "noun" ? 1.0 : AppTheme.opacityDisabled
            enabled: root.editingPartOfSpeech === "noun"

            DesktopSelect
            {
                objectName: "word_dialog_gender"

                heading: qsTr("Gender")
                options: root.genderOptions.map(Option => Option.label)
                currentIndex: root.genderOptions.findIndex(Option => Option.value === root.editingGender)

                onChosen: function(index)
                {
                    root.editingGender = root.genderOptions[index].value
                    root.readingChosen = true
                }
            }
        }
    }

    DesktopFormField
    {
        visible: root.meanings.length > 0
        label: qsTr("Meaning")

        DesktopSelect
        {
            objectName: "word_dialog_meaning"

            heading: qsTr("Meaning")
            visibleRows: 5
            options: root.meaningOptions.map(Gist => Gist.length > 0 ? Gist : qsTr("Any meaning"))
            descriptions: root.meaningOptions.map(Gist =>
            {
                const found = root.meanings.find(Meaning => Meaning.gist === Gist)

                return found ? found.translations.join(", ") : qsTr("The card covers multiple meanings")
            })
            trailingTexts: root.meaningOptions.map(Gist =>
            {
                const found = root.meanings.find(Meaning => Meaning.gist === Gist)

                return found && found.cardId ? qsTr("In My Vocabulary") : ""
            })
            currentIndex: root.meaningOptions.indexOf(root.meaningGist)

            onChosen: function(index) { root.chooseMeaning(root.meaningOptions[index]) }
        }

        Rectangle
        {
            Layout.fillWidth: true
            Layout.topMargin: AppMetrics.gapSmall
            implicitHeight: meaningStack.implicitHeight + meaningStack.anchors.margins * 2
            radius: AppMetrics.radiusMedium
            color: AppTheme.colourSunken

            StackLayout
            {
                id: meaningStack

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: root.expanded ? AppMetrics.cardPadding : AppMetrics.gapMedium
                currentIndex: root.meanings.findIndex(Meaning => Meaning.gist === root.meaningGist)

                Repeater
                {
                    model: root.meanings

                    delegate: Item
                    {
                        id: meaningPage

                        required property var modelData

                        implicitHeight: meaningDetails.implicitHeight

                        DesktopMeaningDetails
                        {
                            id: meaningDetails

                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            meaning: meaningPage.modelData

                            onWordClicked: function(word, sentence) { root.wordClicked(word, sentence) }
                        }
                    }
                }
            }

            DesktopLabel
            {
                anchors.centerIn: parent
                visible: !root.hasMeaning
                text: qsTr("The card covers multiple meanings")
                color: AppTheme.colourMuted
            }
        }
    }

    DesktopFormField
    {
        label: qsTr("Hint")

        DesktopTextField
        {
            id: hintField

            objectName: "word_dialog_hint"
            Layout.fillWidth: true
            Accessible.name: qsTr("Hint")
        }
    }

    DesktopFormField
    {
        id: suggestionSection

        readonly property int reservedRows: 3

        visible: !root.expanded || root.suggestions.length > 0
        opacity: root.suggestions.length > 0 ? 1 : 0
        label: qsTr("Translations")
        trailingLabel: root.suggestions.length

        Flickable
        {
            id: suggestionFlick

            objectName: "word_dialog_translations"

            readonly property real ringMargin: AppMetrics.focusRingWidth * 2
            readonly property real scrollGutter: root.expanded ? 0 : AppMetrics.scrollBarWidth + AppMetrics.gapSmall

            Layout.fillWidth: true
            Layout.leftMargin: -suggestionFlick.ringMargin
            Layout.rightMargin: -suggestionFlick.ringMargin - suggestionFlick.scrollGutter
            Layout.topMargin: -suggestionFlick.ringMargin
            Layout.bottomMargin: -suggestionFlick.ringMargin
            Layout.preferredHeight: root.expanded
                ? suggestionFlick.contentHeight
                : AppMetrics.buttonHeightLarge * suggestionSection.reservedRows
                  + suggestionGrid.rowSpacing * (suggestionSection.reservedRows - 1)
                  + suggestionFlick.ringMargin * 2
            contentWidth: width
            contentHeight: suggestionGrid.implicitHeight + suggestionFlick.ringMargin * 2
            interactive: !root.expanded
            boundsBehavior: Flickable.StopAtBounds
            clip: true

            ScrollBar.vertical: DesktopScrollBar { visible: !root.expanded }

            GridLayout
            {
                id: suggestionGrid

                x: suggestionFlick.ringMargin
                y: suggestionFlick.ringMargin
                width: suggestionFlick.width - suggestionFlick.ringMargin * 2 - suggestionFlick.scrollGutter
                columns: root.expanded ? 3 : 2
                columnSpacing: AppMetrics.gapSmall
                rowSpacing: AppMetrics.gapSmall

                Repeater
                {
                    model: root.suggestions

                    delegate: DesktopTranslationChip
                    {
                        required property var modelData

                        Layout.preferredWidth: 1
                        text: modelData.text
                        relative: modelData.relative
                        hasProbability: modelData.hasProbability
                        learnedStage: modelData.learnedStage
                        inUse: root.hasTranslation(modelData.text)

                        onClicked: root.toggleTranslation(modelData.text)
                    }
                }
            }
        }
    }
}
