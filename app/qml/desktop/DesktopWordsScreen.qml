import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item
{
    id: root

    required property DictionaryController entryDictionary
    required property DictionaryController dictionary
    required property WordListController wordList

    required property var store

    property string source: "history"
    property int selectedIndex: -1
    property string pendingPartOfSpeech: ""

    signal addWordRequested(string word, string translation, string partOfSpeech, string meaningGist)
    signal deckOpenRequested(int deckId, string word, string partOfSpeech)
    signal editWordRequested(string cardId, string word, string translation, int gender,
                             string partOfSpeech, int box)

    readonly property string term: searchField.term
    readonly property bool searching: searchField.term.length > 0
    readonly property bool dueSource: root.source.indexOf("due:") === 0
    readonly property bool phraseTranslated: AppMachineTranslation.enabled && root.searching
        && root.dictionary.results.length === 0 && (root.dictionary.translating || root.dictionary.machineTranslation.length > 0)

    readonly property string speechText: !root.searching ? ""
        : root.dictionary.nativeSearch ? root.dictionary.machineTranslation
        : searchField.term

    readonly property var sources: [
        { key: "history", label: qsTr("Recent searches") },
        { key: "studied", label: qsTr("Recently studied") }
    ]

    readonly property string sourceTitle:
    {
        if (root.dueSource)
        {
            const days = parseInt(root.source.substring(4))

            if (days <= 0)
            {
                return qsTr("Due today")
            }

            const date = new Date()
            date.setDate(date.getDate() + days)

            return qsTr("Due %1").arg(Qt.locale(AppLocalization.localeName).toString(date, "dddd"))
        }

        return root.source === "studied" ? qsTr("Recently studied") : qsTr("Recent searches")
    }

    readonly property var rows: root.currentRows()

    readonly property var selectedRow: root.selectedIndex >= 0 && root.selectedIndex < root.rows.length
        ? root.rows[root.selectedIndex] : null

    function currentRows()
    {
        return searchField.term.length > 0 ? root.dictionary.results : root.wordList.entries
    }

    function rowWord(row)
    {
        return row.targetWord !== undefined ? row.targetWord : row.word
    }

    function setLanguage(language)
    {
        root.dictionary.language = language
        root.entryDictionary.language = language
    }

    function searchTerm(term, language)
    {
        if (language !== undefined)
        {
            root.setLanguage(language)
        }

        searchField.text = term
        root.dictionary.search(term)
        root.selectedIndex = -1
        root.autoSelect()
    }

    function showWord(word, partOfSpeech)
    {
        root.pendingPartOfSpeech = partOfSpeech
        root.searchTerm(word)
    }

    function openList(newSource)
    {
        root.source = newSource
        searchField.text = ""
        root.wordList.load(newSource)
        root.selectedIndex = -1
        root.autoSelect()
    }

    function focusSearch()
    {
        searchField.forceActiveFocus()
        searchField.selectAll()
    }

    function refresh()
    {
        if (root.searching)
        {
            root.dictionary.search(searchField.term)
        }
        else
        {
            root.wordList.load(root.source)
        }

        if (root.selectedIndex < 0 || root.selectedIndex >= root.currentRows().length)
        {
            root.selectedIndex = -1
            root.autoSelect()

            return
        }

        entry.refresh()
    }

    function autoSelect()
    {
        if (root.currentRows().length === 0)
        {
            root.selectedIndex = -1
            entry.clear()

            return
        }

        if (root.selectedIndex < 0)
        {
            root.select(0, false)
        }
    }

    function select(index, record)
    {
        const rows = root.currentRows()

        if (index < 0 || index >= rows.length)
        {
            return
        }

        root.selectedIndex = index
        list.positionViewAtIndex(index, ListView.Contain)

        const row = rows[index]

        entry.open(root.rowWord(row), "", record, root.term)
    }

    Connections
    {
        target: root.dictionary

        function onResultsChanged()
        {
            if (!root.searching)
            {
                return
            }

            root.selectedIndex = -1
            root.autoSelect()

            if (root.pendingPartOfSpeech.length > 0 && root.selectedRow)
            {
                entry.selectPartOfSpeech(root.pendingPartOfSpeech)
                root.pendingPartOfSpeech = ""
            }
        }
    }

    Component.onCompleted:
    {
        root.wordList.load(root.source)

        if (root.dictionary.query.length > 0)
        {
            searchField.text = root.dictionary.query
        }

        root.autoSelect()
    }

    DesktopSidePane
    {
        id: listPane

        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        widthKey: "wordsList"
        defaultWidth: AppMetrics.listPaneWidth

        DesktopSearchArea
        {
            id: searchField

            objectName: "words_search"

            Layout.fillWidth: true
            large: true
            navigatesList: true
            placeholderText: qsTr("Search the dictionary")

            onTermChanged:
            {
                if (searchField.term.length > 0)
                {
                    root.dictionary.search(searchField.term)

                    return
                }

                root.dictionary.clear()
                root.selectedIndex = -1
                root.autoSelect()
            }

            onNextRequested: root.select(root.selectedIndex + 1, false)
            onPreviousRequested: root.select(root.selectedIndex - 1, false)
            onSubmitted: root.select(Math.max(root.selectedIndex, 0), true)
        }

        DesktopChipRow
        {
            spacing: AppMetrics.gapTight
            visible: !root.searching

            Accessible.role: Accessible.ToolBar
            Accessible.name: qsTr("Word lists")

            Repeater
            {
                model: root.sources

                delegate: DesktopChip
                {
                    required property var modelData

                    text: modelData.label
                    selected: root.source === modelData.key

                    onClicked: root.openList(modelData.key)
                }
            }

            DesktopChip
            {
                visible: root.dueSource
                text: root.sourceTitle
                selected: true
            }
        }

        RowLayout
        {
            Layout.fillWidth: true
            spacing: AppMetrics.gapTight

            DesktopSpeakButton
            {
                objectName: "words_search_listen"

                visible: root.searching && root.speechText.length > 0
                focusPolicy: Qt.NoFocus
                iconColour: AppTheme.colourMuted
                text: root.dictionary.nativeSearch ? qsTr("Listen to the translation") : qsTr("Listen")

                onSpeakRequested: root.dictionary.speak(root.speechText)
            }

            DesktopIconButton
            {
                objectName: "words_search_copy"

                visible: root.searching && root.dictionary.machineTranslation.length > 0 && !root.dictionary.translating
                focusPolicy: Qt.NoFocus
                iconName: "copy"
                iconColour: AppTheme.colourMuted
                text: qsTr("Copy translation")

                onClicked: root.dictionary.copyToClipboard(root.dictionary.machineTranslation)
            }

            DesktopLabel
            {
                Layout.fillWidth: true
                visible: !root.phraseTranslated && !(root.searching && root.dictionary.results.length === 0 && AppMachineTranslation.offered)
                text: root.searching ? qsTr("%n results", "", root.dictionary.results.length)
                                     : qsTr("%n words", "", root.wordList.entries.length)
                font.pixelSize: AppMetrics.fontSmall
                color: AppTheme.colourMuted
            }

            Item
            {
                Layout.fillWidth: true
                visible: root.phraseTranslated || (root.searching && root.dictionary.results.length === 0 && AppMachineTranslation.offered)
            }

            DesktopChip
            {
                objectName: "words_search_foreign"

                visible: root.searching
                text: root.dictionary.targetLanguageCode
                selected: !root.dictionary.nativeSearch

                onClicked: root.dictionary.nativeSearch = false
            }

            DesktopChip
            {
                objectName: "words_search_native"

                visible: root.searching
                text: root.dictionary.nativeLanguageCode
                selected: root.dictionary.nativeSearch

                onClicked: root.dictionary.nativeSearch = true
            }
        }

        DesktopMachineTranslation
        {
            visible: AppMachineTranslation.enabled && root.searching
                && (root.dictionary.results.length === 0 || root.dictionary.formTerm.length > 0)
            lookup: root.dictionary
            term: searchField.term

            onAddRequested: function(word, translation) { root.addWordRequested(word, translation, "", "") }
        }

        DesktopWordList
        {
            id: list

            objectName: "words_list"

            emptyText: root.searching && !root.phraseTranslated && !AppMachineTranslation.offered ? qsTr("Nothing found") : ""

            Accessible.name: root.searching ? qsTr("Search results") : root.sourceTitle

            model: root.rows

            delegate: DesktopWordRow
            {
                required property var modelData
                required property int index

                word: modelData.displayWord || modelData.word
                translation: modelData.translation || ""
                gender: modelData.gender ?? -1
                verbGroup: modelData.verbGroup || ""
                cefrLevel: modelData.cefrLevel || ""
                stage: modelData.stage !== undefined ? modelData.stage : -1
                completed: !!modelData.completed
                maxStage: root.store.stageCount
                selected: index === root.selectedIndex

                onClicked: root.select(index, true)
            }
        }
    }

    DesktopDictionaryEntry
    {
        id: entry

        controller: root.entryDictionary

        anchors.left: listPane.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom

        stageCount: root.store.stageCount
        emptyText: ""

        onAddWordRequested: function(word, translation, partOfSpeech, meaningGist) { root.addWordRequested(word, translation, partOfSpeech, meaningGist) }
        onWordClicked: function(word) { root.showWord(word, "") }
        onDeckOpenRequested: function(deckId, word, partOfSpeech) { root.deckOpenRequested(deckId, word, partOfSpeech) }
        onEditWordRequested: function(cardId, word, translation, gender, partOfSpeech, box)
        {
            root.editWordRequested(cardId, word, translation, gender, partOfSpeech, box)
        }
    }
}
