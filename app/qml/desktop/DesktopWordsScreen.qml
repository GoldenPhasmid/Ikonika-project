import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item
{
    id: root

    required property var store

    property string source: "history"
    property int selectedIndex: -1
    property string pendingPartOfSpeech: ""

    signal addWordRequested(string word, string translation, string partOfSpeech, string meaningGist)
    signal editWordRequested(string cardId, string word, string translation, int gender,
                             string partOfSpeech, int box)

    readonly property string term: searchField.term
    readonly property bool searching: searchField.term.length > 0
    readonly property bool dueSource: root.source.indexOf("due:") === 0
    readonly property bool phraseTranslated: AppMachineTranslation.enabled && root.searching
        && dictionary.results.length === 0 && (dictionary.translating || dictionary.machineTranslation.length > 0)

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
        return searchField.term.length > 0 ? dictionary.results : wordList.entries
    }

    function rowWord(row)
    {
        return row.targetWord !== undefined ? row.targetWord : row.word
    }

    function searchTerm(term)
    {
        searchField.text = term
        dictionary.search(term)
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
        wordList.load(newSource)
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
            dictionary.search(searchField.term)
        }
        else
        {
            wordList.load(root.source)
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

    DictionaryController
    {
        id: dictionary

        onResultsChanged:
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

    WordListController
    {
        id: wordList
    }

    Component.onCompleted:
    {
        wordList.load(root.source)
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
                    dictionary.search(searchField.term)

                    return
                }

                dictionary.clear()
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

            DesktopLabel
            {
                Layout.fillWidth: true
                visible: !root.phraseTranslated && !(root.searching && dictionary.results.length === 0 && AppMachineTranslation.offered)
                text: root.searching ? qsTr("%n results", "", dictionary.results.length)
                                     : qsTr("%n words", "", wordList.entries.length)
                font.pixelSize: AppMetrics.fontSmall
                color: AppTheme.colourMuted
            }

            Item
            {
                Layout.fillWidth: true
                visible: root.phraseTranslated || (root.searching && dictionary.results.length === 0 && AppMachineTranslation.offered)
            }

            DesktopChip
            {
                objectName: "words_search_foreign"

                visible: root.searching
                text: dictionary.targetLanguageCode
                selected: !dictionary.nativeSearch

                onClicked: dictionary.nativeSearch = false
            }

            DesktopChip
            {
                objectName: "words_search_native"

                visible: root.searching
                text: dictionary.nativeLanguageCode
                selected: dictionary.nativeSearch

                onClicked: dictionary.nativeSearch = true
            }
        }

        DesktopMachineTranslation
        {
            visible: AppMachineTranslation.enabled && root.searching && dictionary.results.length === 0
            lookup: dictionary
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

        anchors.left: listPane.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom

        stageCount: root.store.stageCount
        emptyText: ""

        onAddWordRequested: function(word, translation, partOfSpeech, meaningGist) { root.addWordRequested(word, translation, partOfSpeech, meaningGist) }
        onWordClicked: function(word) { root.showWord(word, "") }
        onEditWordRequested: function(cardId, word, translation, gender, partOfSpeech, box)
        {
            root.editWordRequested(cardId, word, translation, gender, partOfSpeech, box)
        }
    }
}
