import QtQuick

Item
{
    id: root

    required property DictionaryController controller

    readonly property alias word: entry.word
    readonly property alias partOfSpeech: entry.partOfSpeech
    property int stageCount: 1

    signal done()
    signal addWordRequested(string word, string translation, string partOfSpeech, string meaningGist)
    signal editWordRequested(string cardId, string word, string translation, int gender,
                             string partOfSpeech, int box)
    signal searchRequested()
    signal deckOpenRequested(int deckId, string word, string partOfSpeech)

    function open(headword, pos, query)
    {
        entry.open(headword, pos, true, query || "")
        root.controller.speakAutomatically(headword)
    }

    function refreshLearnedMarks()
    {
        entry.refresh()
    }

    MobileShell
    {
        id: shell

        anchors.fill: parent

        activeTab: "dictionary"
        headerTitle: ""
        showBack: true
        showSearch: true
        showTabBar: false

        onBackRequested: root.done()
        onSearchRequested: root.searchRequested()

        DesktopDictionaryEntry
        {
            id: entry

            anchors.fill: parent

            controller: root.controller
            stageCount: root.stageCount

            onAddWordRequested: function(word, translation, partOfSpeech, meaningGist)
            {
                root.addWordRequested(word, translation, partOfSpeech, meaningGist)
            }

            onEditWordRequested: function(cardId, word, translation, gender, partOfSpeech, box)
            {
                root.editWordRequested(cardId, word, translation, gender, partOfSpeech, box)
            }

            onWordClicked: function(word) { root.open(word, "", "") }
            onDeckOpenRequested: function(deckId, word, partOfSpeech) { root.deckOpenRequested(deckId, word, partOfSpeech) }
        }
    }
}
