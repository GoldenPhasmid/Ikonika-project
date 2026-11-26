import QtQuick
import QtQuick.Layouts

Item
{
    id: root

    required property DictionaryController dictionary

    property var store: null
    property var controller: null

    property var card: null
    property bool actionsOpen: false
    property bool dictionaryForm: false

    readonly property bool portraitDictionaryForm: root.dictionaryForm && root.height > root.width

    signal done()
    signal added()
    signal removed(string cardId)
    signal dictionaryRequested(string word, string partOfSpeech)

    onPortraitDictionaryFormChanged:
    {
        if (!root.portraitDictionaryForm && cardSelectionSheet.visible)
        {
            cardSelectionSheet.close()
        }
    }

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

    function openWith(word, translation, partOfSpeech, gist, hint, language, customAdd)
    {
        root.actionsOpen = false
        root.card = {
            inVocabulary: false,
            word: word,
            targetWord: word,
            translation: translation,
            gender: -1,
            partOfSpeech: partOfSpeech || "",
            box: -1,
            completed: false,
            gist: gist || "",
            hint: hint || "",
            language: language || "",
            customAdd: !!customAdd
        }
        cardPane.show(root.card, true)
    }

    function dismissPortraitSheet()
    {
        if (!root.portraitDictionaryForm)
        {
            return false
        }

        if (cardSelectionSheet.visible)
        {
            cardSelectionSheet.close()
            return true
        }

        if (root.actionsOpen)
        {
            root.actionsOpen = false
            return true
        }

        return false
    }

    MobileShell
    {
        id: shell

        anchors.fill: parent

        activeTab: "dictionary"
        headerTitle: ""
        showBack: true
        showOptions: !root.portraitDictionaryForm && root.card !== null && root.card.word.length > 0
        showTabBar: false

        onBackRequested: root.done()
        onOptionsRequested: root.actionsOpen = true

        headerStatus: RowLayout
        {
            visible: root.portraitDictionaryForm && cardPane.actionWord.length > 0
            spacing: 0

            DesktopSpeakButton
            {
                objectName: "word_dialog_speak"
                text: qsTr("Pronounce")

                onSpeakRequested: cardPane.speak()
            }

            MobileIconButton
            {
                objectName: "word_dialog_dictionary"
                iconName: "nav_words"
                text: qsTr("Open in Dictionary")

                onClicked: cardPane.openInWords()
            }

            MobileIconButton
            {
                objectName: "word_dialog_web"
                iconName: "external_link"
                text: qsTr("Search the web")

                onClicked: cardPane.searchWeb()
            }
        }

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
            selectionPicker: root.portraitDictionaryForm ? cardSelectionSheet : null

            onActionsRequested: root.actionsOpen = true

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
            showLookupActions: !root.portraitDictionaryForm

            onDismissed: root.actionsOpen = false
        }
    }

    MobileSettingsPicker
    {
        id: cardSelectionSheet

        anchors.fill: parent
        z: 10
    }
}
