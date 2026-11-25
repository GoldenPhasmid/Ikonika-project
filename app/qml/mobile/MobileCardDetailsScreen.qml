import QtQuick

Item
{
    id: root

    required property DictionaryController dictionary

    property var store: null
    property var controller: null

    property var card: null
    property bool actionsOpen: false

    signal done()
    signal added()
    signal removed(string cardId)
    signal dictionaryRequested(string word, string partOfSpeech)

    function openForEdit(cardId, word, translation, gender, partOfSpeech, box, language)
    {
        root.actionsOpen = false
        root.card = {
            inVocabulary: true,
            id: cardId,
            word: word,
            targetWord: word,
            translation: translation,
            gender: gender,
            partOfSpeech: partOfSpeech,
            box: box,
            completed: false,
            language: language || ""
        }
        cardPane.show(root.card, true)

        const stored = root.dictionary.cards.find(Card => Card.cardId === cardId)

        if (stored && stored.completed)
        {
            root.card = Object.assign({}, root.card, { completed: true })
        }
    }

    function openWith(word, translation, partOfSpeech, gist, hint, language)
    {
        root.actionsOpen = false
        root.card = {
            inVocabulary: false,
            word: word,
            targetWord: word,
            translation: translation,
            gender: -1,
            partOfSpeech: partOfSpeech || "",
            gist: gist || "",
            hint: hint || "",
            language: language || ""
        }
        cardPane.show(root.card, true)
    }

    MobileShell
    {
        id: shell

        anchors.fill: parent

        activeTab: "dictionary"
        headerTitle: ""
        showBack: true
        showOptions: root.card !== null && root.card.word.length > 0
        showTabBar: false

        onBackRequested: root.done()
        onOptionsRequested: root.actionsOpen = true

        DesktopCardPane
        {
            id: cardPane

            anchors.fill: parent

            dictionary: root.dictionary
            controller: root.controller
            card: root.card
            stageCount: root.store ? root.store.stageCount : 1
            compact: true
            actionsButtonShown: false

            onSaved:
            {
                root.added()
                root.done()
            }

            onProgressEdited: function(card)
            {
                root.card = card
                root.added()
            }

            onRemoveRequested: function(cardId, word)
            {
                if (!root.controller.removeCard(cardId))
                {
                    console.warn("Card screen could not remove", word)

                    return
                }

                console.info("Card screen removed", word)

                root.removed(cardId)
                root.added()
                root.done()
            }

            onDictionaryRequested: function(word, partOfSpeech) { root.dictionaryRequested(word, partOfSpeech) }
        }
    }

    MobileSheetLoader
    {
        anchors.fill: parent
        z: 10
        open: root.actionsOpen

        sourceComponent: MobileCardActionsSheet
        {
            pane: cardPane

            onDismissed: root.actionsOpen = false
        }
    }
}
