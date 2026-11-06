import QtQuick
import QtQuick.Layouts
import "MobileIcons.js" as Icons

Item
{
    id: root

    property var store: null
    property var controller: null

    property string meaningGist: ""

    readonly property var meanings: (dictionary.entry.headword || "").toLowerCase() === wordField.enteredText.trim().toLowerCase()
        && (dictionary.entry.partOfSpeech || "") === root.shownPartOfSpeech
        ? (dictionary.entry.meanings || []) : []
    readonly property var meaning: root.meanings.find(function(item) { return item.gist === root.meaningGist }) || ({})
    readonly property bool hasMeaning: !!root.meaning.gist
    readonly property string committedGist: root.hasMeaning || (root.editingCardId.length > 0 && root.meanings.length === 0)
        ? root.meaningGist : ""
    readonly property var meaningOptions: [""].concat(root.meanings.map(function(item) { return item.gist }))

    property string editingCardId: ""
    property int editingStage: -1
    property string adoptedCardId: ""
    property string returnCardId: ""
    property int returnStage: -1
    property int returnGender: -1
    property string returnPartOfSpeech: ""
    property string returnHint: ""
    readonly property var editingCard: root.editingCardId.length > 0
        ? (dictionary.cards.find(function(card) { return card.cardId === root.editingCardId }) || null) : null

    property bool startOnTranslation: false
    property int editingGender: -1
    property string editingPartOfSpeech: ""

    property bool readingChosen: false

    readonly property bool followsDictionary: root.editingCardId.length === 0 && !root.readingChosen

    readonly property string shownPartOfSpeech: root.followsDictionary
        ? root.dictionaryPartOfSpeech
        : root.editingPartOfSpeech

    readonly property int shownGender: root.followsDictionary
        ? root.dictionaryGender
        : root.editingGender

    readonly property bool showGenderPicker: root.shownPartOfSpeech === "noun"

    readonly property var genderByCode: ({ "m": 0, "f": 1, "m/f": 2 })

    readonly property var genderOptions: [
        { label: qsTr("Masculine"), value: 0 },
        { label: qsTr("Feminine"), value: 1 },
        { label: qsTr("Either (m/f)"), value: 2 },
        { label: qsTr("None (no article)"), value: -1 }
    ]

    function partOfSpeechLabel(value)
    {
        const found = dictionary.partsOfSpeech.find(function(option) { return option.value === value })

        if (found)
        {
            return found.label
        }

        return value.length > 0 ? value : qsTr("None")
    }

    function chooseReading(partOfSpeech, gender)
    {
        const changed = partOfSpeech !== root.shownPartOfSpeech

        root.editingPartOfSpeech = partOfSpeech
        root.editingGender = partOfSpeech === "noun" ? gender : -1
        root.readingChosen = true

        if (!changed || wordField.enteredText.trim().length === 0)
        {
            return
        }

        dictionary.openEntry(wordField.enteredText.trim(), partOfSpeech)

        if (!root.hasMeaning)
        {
            root.meaningGist = ""
        }

        root.rebuildSuggestions()
    }

    function genderLabel(value)
    {
        const found = root.genderOptions.find(function(option) { return option.value === value })

        return found ? found.label : qsTr("None (no article)")
    }

    signal done()
    signal added()
    signal wordTapped(string word, string sentence)

    readonly property var pickedTranslations:
        translationField.enteredText.split(",")
            .map(function(part) { return part.trim().toLowerCase() })
            .filter(function(part) { return part.length > 0 })

    property var suggestions: []

    function open()
    {
        root.openWith("", "", "")
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
    }

    function findMeaningFor(translation)
    {
        const wanted = (translation || "").trim().toLowerCase()

        if (wanted.length === 0)
        {
            return root.meanings.length > 0 ? root.meanings[0].gist : ""
        }

        const found = root.meanings.find(function(item)
        {
            return item.translations.some(function(text) { return text.trim().toLowerCase() === wanted })
        })

        return found ? found.gist : ""
    }

    function meaningLabel(gist)
    {
        return gist.length > 0 ? gist : qsTr("Any meaning")
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

            console.info("Card screen edits the card of meaning", chosen.gist, cardId)

            root.adoptedCardId = cardId
            root.editingPartOfSpeech = dictionary.entry.partOfSpeech || ""
            root.editingStage = chosen.learnedStage
            root.editingGender = chosen.cardGender
            root.editingCardId = cardId
            hintField.text = root.returnHint || dictionary.cardHint(cardId)
            translationField.replaceText(chosen.cardTranslation)

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

    function chooseMeaning(gist)
    {
        root.meaningGist = gist
        root.followMeaningCard()

        if (!root.hasMeaning || root.meaning.cardId)
        {
            root.rebuildSuggestions()

            return
        }

        if (root.meaning.gender >= 0 && root.shownPartOfSpeech === "noun")
        {
            root.chooseReading("noun", root.meaning.gender)
        }

        const allowed = root.meaning.translations.map(function(text) { return text.trim().toLowerCase() })
        const kept = translationField.enteredText.split(",")
            .map(function(part) { return part.trim() })
            .filter(function(part) { return allowed.indexOf(part.toLowerCase()) >= 0 })

        translationField.replaceText(kept.length > 0 ? kept.join(", ") : (root.meaning.translations[0] || ""))
        root.rebuildSuggestions()
    }

    function pickMeaning()
    {
        choiceSheet.open(qsTr("Meaning"),
                       root.meaningOptions.map(root.meaningLabel),
                       root.meaningOptions.indexOf(root.meaningGist),
                       function(index) { root.chooseMeaning(root.meaningOptions[index]) },
                       root.meaningOptions.map(function(gist)
                       {
                           const found = root.meanings.find(function(item) { return item.gist === gist })

                           return found ? found.translations.join(", ") : qsTr("The card covers multiple meanings")
                       }),
                       root.meaningOptions.map(function(gist)
                       {
                           const found = root.meanings.find(function(item) { return item.gist === gist })

                           return found && found.cardId ? qsTr("In My Vocabulary") : ""
                       }))
    }

    function openWith(word, translation, partOfSpeech, gist, hint)
    {
        root.editingCardId = ""
        root.editingStage = -1
        root.editingGender = -1
        root.editingPartOfSpeech = ""
        root.adoptedCardId = ""
        root.readingChosen = false
        root.meaningGist = ""

        wordField.text = word
        translationField.text = translation
        hintField.text = hint || ""
        root.startOnTranslation = word.length > 0
        root.suggestions = []
        dictionary.openEntry("", "")

        if (word.length > 0)
        {
            dictionary.openEntry(word, partOfSpeech || "")
            root.meaningGist = gist || root.findMeaningFor(translation)

            if (root.hasMeaning && root.meaning.gender >= 0 && root.dictionaryPartOfSpeech === "noun")
            {
                root.chooseReading("noun", root.meaning.gender)
            }

            root.followMeaningCard()

            if (translationField.text.length === 0 && root.hasMeaning)
            {
                translationField.text = root.meaning.translations[0] || ""
            }

            root.rebuildSuggestions()
        }
    }

    function hasTranslation(text)
    {
        return root.pickedTranslations.indexOf(text.trim().toLowerCase()) >= 0
    }

    function toggleTranslation(text)
    {
        const wanted = text.trim().toLowerCase()

        const entered = translationField.enteredText

        if (!root.hasTranslation(text))
        {
            translationField.replaceText(entered.trim().length > 0
                ? entered.trim() + ", " + text
                : text)

            return
        }

        translationField.replaceText(entered.split(",")
            .filter(function(part) { return part.trim().toLowerCase() !== wanted })
            .map(function(part) { return part.trim() })
            .join(", "))
    }

    function canCommit()
    {
        return wordField.enteredText.trim().length > 0 && translationField.enteredText.trim().length > 0
    }

    function commit()
    {
        if (!root.canCommit())
        {
            return
        }

        const written = root.editingCardId.length > 0
            ? root.controller.updateCard(root.editingCardId, wordField.enteredText,
                                         translationField.enteredText, root.editingGender,
                                         root.editingPartOfSpeech, root.committedGist, hintField.text.trim())
            : root.controller.addCard(wordField.enteredText, translationField.enteredText,
                                      root.shownPartOfSpeech, root.shownGender, root.readingChosen,
                                      root.committedGist, hintField.text.trim())

        if (written)
        {
            root.added()
            root.done()
        }
    }

    function rebuildSuggestions()
    {
        const result = []

        const meaningTranslations = root.hasMeaning
            ? root.meaning.translations.map(function(text) { return text.trim().toLowerCase() }) : []

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
                    ? root.controller.findWordTranslationStage(wordField.text, translation.text)
                    : translation.learnedStage,
            })
        }

        root.suggestions = result
    }

    readonly property string dictionaryPartOfSpeech:
        (dictionary.entry.headword || "").toLowerCase() === wordField.enteredText.trim().toLowerCase()
            ? (dictionary.entry.partOfSpeech || "")
            : ""

    readonly property int dictionaryGender:
    {
        if (root.dictionaryPartOfSpeech !== "noun")
        {
            return -1
        }

        const gendered = (dictionary.entry.senses || []).find(
            function(sense) { return sense.gender in root.genderByCode })

        return gendered ? root.genderByCode[gendered.gender] : -1
    }

    DictionaryController
    {
        id: dictionary
    }

    MobileShell
    {
        anchors.fill: parent

        activeTab: "dictionary"
        headerTitle: root.editingCardId.length > 0 ? qsTr("Edit card") : qsTr("Create card")
        headerStatus: MobileProgressRing
        {
            visible: root.editingCardId.length > 0
            stage: root.editingCard ? root.editingCard.stage : root.editingStage
            stageCount: root.store ? root.store.stageCount : 1
            completed: root.editingCard ? root.editingCard.completed : false
        }
        showBack: true
        showTabBar: false

        onBackRequested: root.done()

        Flickable
        {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: wideButton.top
            anchors.bottomMargin: AppMetrics.gapSmall

            contentWidth: width
            contentHeight: content.implicitHeight + AppMetrics.gapLarge * 2
            boundsBehavior: Flickable.StopAtBounds
            clip: true

            ColumnLayout
            {
                id: content

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: AppMetrics.gapLarge

                spacing: AppMetrics.gapLarge

                MobileFormField
                {
                    id: wordField

                    fieldFocus: root.visible && !root.startOnTranslation

                    label: qsTr("WORD")
                    placeholder: qsTr("word")

                    onAccepted:
                    {
                        dictionary.openEntry(wordField.text, root.editingCardId.length > 0 ? root.editingPartOfSpeech : "")
                        root.rebuildSuggestions()
                    }
                }

                MobileFormField
                {
                    id: translationField

                    fieldFocus: root.visible && root.startOnTranslation

                    label: qsTr("TRANSLATION")
                    placeholder: qsTr("translation")

                    onAccepted: root.commit()
                }

                MobileFormField
                {
                    id: hintField

                    objectName: "word_dialog_hint"
                    label: qsTr("HINT")
                }

                ColumnLayout
                {
                    Layout.fillWidth: true
                    spacing: AppMetrics.gapSmall
                    visible: root.meanings.length > 0

                    MobileFieldPicker
                    {
                        Layout.fillWidth: true

                        label: qsTr("MEANING")
                        value: root.meaningLabel(root.meaningGist)

                        onPicked: root.pickMeaning()
                    }

                    Rectangle
                    {
                        Layout.fillWidth: true
                        visible: root.hasMeaning
                        implicitHeight: meaningDetails.implicitHeight + AppMetrics.gapMedium * 2
                        radius: AppMetrics.radiusMedium
                        color: AppTheme.colourSunken

                        MobileMeaningDetails
                        {
                            id: meaningDetails

                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: AppMetrics.gapMedium
                            meaning: root.meaning

                            onWordTapped: function(word, sentence) { root.wordTapped(word, sentence) }
                        }
                    }
                }

                ColumnLayout
                {
                    Layout.fillWidth: true
                    spacing: AppMetrics.gapSmall
                    visible: root.suggestions.length > 0

                    MobileSectionLabel
                    {
                        text: root.hasMeaning ? qsTr("TRANSLATIONS FOR THIS MEANING") : qsTr("DICTIONARY TRANSLATIONS")
                    }

                    Repeater
                    {
                        model: root.suggestions

                        delegate: MobileTranslationChip
                        {
                            required property var modelData

                            text: modelData.text
                            relative: modelData.relative
                            hasProbability: modelData.hasProbability
                            learnedStage: modelData.learnedStage
                            inUse: root.hasTranslation(modelData.text)

                            onPicked: root.toggleTranslation(modelData.text)
                        }
                    }
                }

                MobileFieldPicker
                {
                    Layout.fillWidth: true

                    label: qsTr("PART OF SPEECH")
                    value: root.partOfSpeechLabel(root.shownPartOfSpeech)

                    onPicked: choiceSheet.open(qsTr("Part of speech"),
                                              dictionary.partsOfSpeech.map(
                                                  function(option) { return option.label }),
                                              dictionary.partsOfSpeech.findIndex(
                                                  function(option) { return option.value === root.shownPartOfSpeech }),
                                              function(index)
                                              {
                                                  root.chooseReading(dictionary.partsOfSpeech[index].value,
                                                                     root.shownGender)
                                              })
                }

                MobileFieldPicker
                {
                    Layout.fillWidth: true
                    visible: root.showGenderPicker

                    label: qsTr("GENDER")
                    value: root.genderLabel(root.shownGender)
                    description: qsTr("Controls the article shown and tested in exercises. Choose None to drop it.")

                    onPicked: choiceSheet.open(qsTr("Gender"),
                                              root.genderOptions.map(
                                                  function(option) { return option.label }),
                                              root.genderOptions.findIndex(
                                                  function(option) { return option.value === root.shownGender }),
                                              function(index)
                                              {
                                                  root.chooseReading(root.shownPartOfSpeech,
                                                                     root.genderOptions[index].value)
                                              })
                }
            }
        }

        MobileWideButton
        {
            id: wideButton

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: AppMetrics.gapLarge

            height: AppMetrics.buttonHeightLarge

            label: root.editingCardId.length > 0 ? qsTr("Save") : qsTr("Add")
            interactive: root.canCommit()

            onClicked: root.commit()
        }
    }

    MobileSettingsPicker
    {
        id: choiceSheet

        anchors.fill: parent
        z: 60
    }
}
