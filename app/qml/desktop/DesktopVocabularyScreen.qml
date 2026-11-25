import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item
{
    id: root

    required property DictionaryController cardDictionary
    required property var store

    required property VocabularyController controller

    property var deckId: 0
    property string userDeckId: ""
    readonly property bool deckMode: root.deckId > 0 || root.userDeckId.length > 0

    property Component deckHeader: null
    property bool compact: false
    readonly property alias pane: pane
    readonly property Item deckHeaderItem: headerLoader.item

    readonly property bool unsaved: pane.unsaved
    readonly property bool typing: pane.typing || searchField.activeFocus
    readonly property bool narrowed: root.controller.search.length > 0 || root.controller.filterKeys.length > 0
    readonly property bool cardsAddable: root.deckId <= 0
    readonly property bool emptyDeck: root.userDeckId.length > 0 && root.controller.filteredCount === 0 && !root.narrowed

    readonly property alias vocabulary: root.controller

    property int selectedIndex: -1

    readonly property var selectedCard: root.selectedIndex >= 0 && root.selectedIndex < root.controller.visibleCards.length
        ? root.controller.visibleCards[root.selectedIndex] : null
    readonly property bool selectedInVocabulary: root.selectedCard !== null && root.selectedCard.inVocabulary

    signal dictionaryRequested(string word, string partOfSpeech)
    signal cardRemoved(string cardId)
    signal removeCardRequested(string cardId, string word)
    signal addCardRequested()
    signal cardActionsRequested()

    readonly property var sortOptions: root.deckMode ? [
        { key: "deck", primary: qsTr("Deck order"), inverted: qsTr("Reverse deck order") },
        { key: "word", primary: qsTr("A to Z"), inverted: qsTr("Z to A") }
    ] : [
        { key: "word", primary: qsTr("A to Z"), inverted: qsTr("Z to A") },
        { key: "created", primary: qsTr("Recently added"), inverted: qsTr("First added") },
        { key: "lastupdated", primary: qsTr("Recently changed"), inverted: qsTr("Oldest change") },
        { key: "due", primary: qsTr("Due soon"), inverted: qsTr("Due later") }
    ]

    readonly property string sortLabel:
    {
        for (const option of root.sortOptions)
        {
            if (option.key === root.controller.sortKey)
            {
                return root.controller.sortDescending ? option.inverted : option.primary
            }
        }

        return qsTr("Sort")
    }

    function resetView()
    {
        root.controller.resetView()
        searchField.text = ""
        root.selectedIndex = root.controller.visibleCards.length > 0 ? 0 : -1
        root.showSelected()
    }

    function focusEntry(word, partOfSpeech)
    {
        if (word.length === 0)
        {
            return
        }

        searchField.text = word

        const cards = root.controller.visibleCards
        const exact = cards.findIndex(Card => Card.targetWord === word && Card.partOfSpeech === partOfSpeech)
        const sameWord = cards.findIndex(Card => Card.targetWord === word)

        root.select(exact >= 0 ? exact : sameWord)
    }

    function refresh()
    {
        const keptId = root.selectedCard ? root.selectedCard.id : ""
        const scrollPosition = list.contentY

        root.controller.refresh()

        root.reselect(keptId)
        root.showSelected(true)
        Qt.callLater(function() { list.contentY = scrollPosition })
    }

    function reselect(cardId)
    {
        const found = root.controller.visibleCards.findIndex(Card => Card.id === cardId)

        root.selectedIndex = found >= 0 ? found : Math.min(root.selectedIndex, root.controller.visibleCards.length - 1)
        root.showSelected()
    }

    function showSelected(force)
    {
        pane.show(root.selectedCard, force === true)
    }

    function select(index)
    {
        if (index < 0 || index >= root.controller.visibleCards.length)
        {
            return
        }

        root.selectedIndex = index
        list.positionViewAtIndex(index, ListView.Contain)
        root.showSelected()
    }

    function focusSearch()
    {
        searchField.forceActiveFocus()
        searchField.selectAll()
    }

    function editCard(index)
    {
        root.select(index)
        pane.focusField()
    }

    function removeCard(cardId)
    {
        if (root.controller.removeCard(cardId))
        {
            root.cardRemoved(cardId)
            root.reselect("")
        }
    }

    Binding
    {
        target: root.controller
        property: "deckId"
        value: root.deckId
    }

    Binding
    {
        target: root.controller
        property: "userDeckId"
        value: root.userDeckId
    }

    Connections
    {
        target: root.controller

        function onViewChanged()
        {
            Qt.callLater(function()
            {
                if (root.selectedIndex >= root.controller.visibleCards.length)
                {
                    root.selectedIndex = root.controller.visibleCards.length - 1
                }

                if (root.selectedIndex < 0 && root.controller.visibleCards.length > 0)
                {
                    root.selectedIndex = 0
                }

                root.showSelected()
            })
        }
    }

    DesktopSidePane
    {
        id: listPane

        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        widthKey: "vocabularyList"
        defaultWidth: AppMetrics.listPaneWidth

        RowLayout
        {
            id: tools

            Layout.fillWidth: true
            spacing: AppMetrics.gapSmall

            DesktopSearchField
            {
                id: searchField

                objectName: "vocabulary_search"

                Layout.fillWidth: true
                placeholderText: qsTr("Search")

                onTermChanged: root.controller.search = searchField.text + searchField.preeditText

                Keys.onDownPressed: root.select(root.selectedIndex + 1)
                Keys.onUpPressed: root.select(root.selectedIndex - 1)
                Keys.onReturnPressed: if (root.selectedCard) root.editCard(root.selectedIndex)

                Connections
                {
                    target: root.controller

                    function onVocabularyChanged() { searchField.text = root.controller.search }
                }
            }

            DesktopButton
            {
                id: sortButton

                objectName: "vocabulary_sort"

                iconOnly: tools.width - fullWidth - tools.spacing < AppMetrics.listSearchMinimumWidth
                iconName: root.controller.sortDescending ? "arrow_up_narrow_wide" : "arrow_down_wide_narrow"
                text: root.sortLabel

                Accessible.name: qsTr("Sort by: %1").arg(root.sortLabel)

                onClicked: sortPopover.open()

                DesktopPopover
                {
                    id: sortPopover

                    x: sortButton.width - width
                    y: sortButton.height + AppMetrics.gapTight
                    heading: qsTr("Sort by")

                    Repeater
                    {
                        model: root.sortOptions

                        delegate: ColumnLayout
                        {
                            required property var modelData

                            Layout.fillWidth: true
                            spacing: 0

                            DesktopPopoverRow
                            {
                                text: modelData.primary
                                showsCheck: true
                                selected: root.controller.sortKey === modelData.key && !root.controller.sortDescending

                                onClicked:
                                {
                                    root.controller.sortKey = modelData.key
                                    root.controller.sortDescending = false
                                    sortPopover.close()
                                }
                            }

                            DesktopPopoverRow
                            {
                                text: modelData.inverted
                                showsCheck: true
                                selected: root.controller.sortKey === modelData.key && root.controller.sortDescending

                                onClicked:
                                {
                                    root.controller.sortKey = modelData.key
                                    root.controller.sortDescending = true
                                    sortPopover.close()
                                }
                            }
                        }
                    }
                }
            }
        }

        MobileVocabularyBar
        {
            readonly property string selectedFilterKey: root.controller.filterKeys.length === 1 ? root.controller.filterKeys[0] : ""

            notStartedCount: root.controller.notLearnedCount
            learningCount: root.controller.learningCount
            learnedCount: root.controller.learnedCount
            selectedKey: selectedFilterKey
            trackShown: !root.deckMode
            joined: true

            onStageOpened: function(filterKey)
            {
                root.controller.filterKeys = filterKey === selectedFilterKey ? [] : [filterKey]
            }
        }

        DesktopLabel
        {
            Layout.fillWidth: true
            visible: !root.deckMode || root.narrowed
            text: qsTr("%n words", "", root.controller.filteredCount)
            font.pixelSize: AppMetrics.fontSmall
            color: AppTheme.colourMuted
        }

        DesktopWordList
        {
            id: list

            objectName: "vocabulary_words"

            emptyText: searchField.text.length > 0 ? qsTr("No words match.")
                     : root.emptyDeck ? "" : qsTr("No words in this view.")
            header: root.cardsAddable ? addCardRow : null

            Accessible.name: qsTr("Words in My Vocabulary")

            model: root.controller.visibleCards

            delegate: DesktopWordRow
            {
                required property var modelData
                required property int index

                word: modelData.word
                translation: modelData.translation
                partOfSpeech: modelData.partOfSpeech
                gender: modelData.gender
                stage: modelData.inVocabulary ? modelData.box : -1
                completed: modelData.completed
                maxStage: root.store.stageCount
                selected: index === root.selectedIndex

                onClicked: root.select(index)
                onDoubleClicked: root.editCard(index)
            }
        }
    }

    Component
    {
        id: addCardRow

        Item
        {
            width: list.width
            height: addCardButton.implicitHeight + list.spacing

            DesktopAddCardRow
            {
                id: addCardButton

                objectName: "vocabulary_add_card"

                width: list.rowWidth

                onClicked: root.addCardRequested()
            }
        }
    }

    Loader
    {
        id: headerLoader

        anchors.left: listPane.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: root.userDeckId.length > 0 ? AppMetrics.gapSmall : 0
        height: item ? item.implicitHeight : 0
        active: root.deckHeader !== null
        sourceComponent: root.deckHeader
    }

    DesktopCardPane
    {
        id: pane

        dictionary: root.cardDictionary

        anchors.left: listPane.right
        anchors.right: parent.right
        anchors.top: headerLoader.bottom
        anchors.bottom: parent.bottom

        removable: root.deckId <= 0
        quiet: root.deckMode
        compact: root.compact
        controller: root.controller
        card: root.selectedCard
        stageCount: root.store.stageCount
        emptyText: root.controller.visibleCards.length === 0
                   ? (root.userDeckId.length > 0 ? qsTr("This deck has no cards yet.")
                                                 : qsTr("Add words to My Vocabulary from Words."))
                   : ""

        onSaved: root.refresh()
        onRemoveRequested: function(cardId, word) { root.removeCardRequested(cardId, word) }
        onDictionaryRequested: function(word, partOfSpeech) { root.dictionaryRequested(word, partOfSpeech) }
        onActionsRequested: root.cardActionsRequested()
    }
}
