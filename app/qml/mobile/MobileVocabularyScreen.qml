import QtQuick
import QtQuick.Layouts

Item
{
    id: root

    required property var store

    property var deckId: 0
    readonly property bool deckMode: root.deckId > 0

    readonly property var vocabulary: controller

    property real pendingScrollPosition: -1

    signal practiceRequested()
    signal addWordRequested()
    signal cardRemoved(string cardId)
    signal editWordRequested(string cardId, string word, string translation, int gender,
                             string partOfSpeech, int box)
    signal wordOpened(string word, string partOfSpeech)
    signal tabSelected(string key)
    signal searchRequested()
    signal learnRequested(var deckId)
    signal backRequested()

    function resetView()
    {
        controller.resetView()
        searchFilterBar.searchText = ""
    }

    function refresh()
    {
        const scrollPosition = root.pendingScrollPosition >= 0
            ? root.pendingScrollPosition : wordList.contentY

        root.pendingScrollPosition = -1

        controller.refresh()
        Qt.callLater(function() { wordList.contentY = scrollPosition })
    }

    VocabularyController
    {
        id: controller

        deckId: root.deckId
    }

    readonly property var filterOptions: [
        { key: "notlearned", label: qsTr("Not learned") },
        { key: "learning", label: qsTr("Learning") },
        { key: "completed", label: qsTr("Learned") }
    ]

    readonly property string selectedFilterKey: controller.filterKeys.length === 1 ? controller.filterKeys[0] : ""

    property bool filterSheetOpen: false

    readonly property string filterLabel:
    {
        if (controller.filterKeys.length === 0)
        {
            return qsTr("All")
        }

        if (controller.filterKeys.length === 1)
        {
            for (const option of root.filterOptions)
            {
                if (option.key === controller.filterKeys[0])
                {
                    return option.label
                }
            }
        }

        return qsTr("%n selected", "", controller.filterKeys.length)
    }

    function toggleFilterKey(key)
    {
        const next = controller.filterKeys.slice()
        const index = next.indexOf(key)

        if (index >= 0)
        {
            next.splice(index, 1)
        }
        else
        {
            next.push(key)
        }

        controller.filterKeys = next
    }

    readonly property var sortOptions: root.deckMode ? [
        { key: "deck", primary: qsTr("Deck order"), inverted: qsTr("Reverse deck order") },
        { key: "word", primary: qsTr("A-Z"), inverted: qsTr("Z-A") }
    ] : [
        { key: "word", primary: qsTr("A-Z"), inverted: qsTr("Z-A") },
        { key: "created", primary: qsTr("Recently added"), inverted: qsTr("First added") },
        { key: "lastupdated", primary: qsTr("Recently changed"), inverted: qsTr("Oldest change") },
        { key: "due", primary: qsTr("Due soon"), inverted: qsTr("Due later") }
    ]

    property bool sortSheetOpen: false

    readonly property bool anySheetOpen: root.filterSheetOpen || root.sortSheetOpen

    readonly property string sortLabel:
    {
        for (const option of root.sortOptions)
        {
            if (option.key === controller.sortKey)
            {
                return controller.sortDescending ? option.inverted : option.primary
            }
        }

        return ""
    }

    readonly property var wordCardModel:
    {
        const rows = []

        for (const card of controller.visibleCards)
        {
            const row = {
                word: card.word,
                translation: card.translation,
                gender: card.gender,
                stage: card.inVocabulary ? card.box : -1,
                completed: card.completed
            }

            if (!card.inVocabulary)
            {
                row.showEditAction = false
                row.showDeleteAction = false
                row.showCompleteAction = false
                row.showResetAction = false
            }

            rows.push(row)
        }

        return rows
    }

    MobileShell
    {
        id: shell

        anchors.fill: parent

        activeTab: root.deckMode ? "decks" : "vocabulary"
        contentMaximumWidth: root.deckMode ? Number.POSITIVE_INFINITY : 672
        headerTitle: root.deckMode ? controller.deckName : qsTr("My Vocabulary")
        showBack: root.deckMode
        showSearch: true
        showTabBar: true

        onTabSelected: function(key) { root.tabSelected(key) }
        onSearchRequested: root.searchRequested()
        onBackRequested: root.backRequested()

        ColumnLayout
        {
            id: content

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: actionRow.top
            anchors.bottomMargin: AppMetrics.gapSmall
            anchors.margins: AppMetrics.contentPadding
            spacing: AppMetrics.gapWide
            enabled: !root.anySheetOpen

            MobileSearchFilterBar
            {
                id: searchFilterBar

                Layout.fillWidth: true

                searchPlaceholder: qsTr("Search words...")
                filterLabel: root.filterLabel
                showFilter: false
                sortLabel: root.sortLabel
                counterText: qsTr("%n words", "", controller.filteredCount)

                onSearchTextChanged: controller.search = searchText + searchPreeditText
                onSearchPreeditTextChanged: controller.search = searchText + searchPreeditText
                onFilterRequested: root.filterSheetOpen = true
                onSortRequested: root.sortSheetOpen = true

                MobileVocabularyBar
                {
                    notStartedCount: controller.notLearnedCount
                    learningCount: controller.learningCount
                    learnedCount: controller.learnedCount
                    selectedKey: root.selectedFilterKey

                    onStageOpened: function(filterKey)
                    {
                        controller.filterKeys = filterKey === root.selectedFilterKey ? [] : [filterKey]
                    }
                }

                Connections
                {
                    target: controller

                    function onVocabularyChanged() { searchFilterBar.searchText = controller.search }
                }
            }

            MobileWordCardList
            {
                id: wordList

                Layout.fillWidth: true
                Layout.fillHeight: true

                model: root.wordCardModel
                bottomInset: 0
                maxStage: root.store.stageCount
                showCompleteAction: true
                showResetAction: true
                showLookAction: true
                showEditAction: false

                onWordClicked: function(index)
                {
                    const card = controller.visibleCards[index]

                    if (!card.inVocabulary)
                    {
                        root.wordOpened(card.targetWord, card.partOfSpeech)

                        return
                    }

                    root.pendingScrollPosition = wordList.contentY
                    root.editWordRequested(card.id, card.targetWord, card.translation,
                                           card.gender, card.partOfSpeech, card.box)
                }

                onLookRequested: function(index)
                {
                    const card = controller.visibleCards[index]
                    root.wordOpened(card.targetWord, card.partOfSpeech)
                }

                onVoicePlayRequested: function(index)
                {
                    controller.speakWord(controller.visibleCards[index].targetWord)
                }

                onEditRequested: function(index)
                {
                    const card = controller.visibleCards[index]
                    root.pendingScrollPosition = wordList.contentY
                    root.editWordRequested(card.id, card.targetWord, card.translation,
                                           card.gender, card.partOfSpeech, card.box)
                }

                onDeleteRequested: function(index)
                {
                    const card = controller.visibleCards[index]
                    const scrollPosition = wordList.contentY

                    if (controller.removeCard(card.id))
                    {
                        root.cardRemoved(card.id)
                        Qt.callLater(function() { wordList.contentY = scrollPosition })
                    }
                }

                onCompleteRequested: function(index)
                {
                    const scrollPosition = wordList.contentY

                    controller.completeCardProgress(controller.visibleCards[index].id)
                    Qt.callLater(function() { wordList.contentY = scrollPosition })
                }

                onResetRequested: function(index)
                {
                    const scrollPosition = wordList.contentY

                    controller.resetCardProgress(controller.visibleCards[index].id)
                    Qt.callLater(function() { wordList.contentY = scrollPosition })
                }
            }
        }

        MobileWideButtonRow
        {
            id: actionRow

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: AppMetrics.contentPadding
            enabled: !root.anySheetOpen

            MobileWideButton
            {
                objectName: "deck_learn"
                visible: root.deckMode

                label: qsTr("Learn")
                interactive: controller.deckNewCount > 0

                onClicked: root.learnRequested(root.deckId)
            }

            MobileWideButton
            {
                visible: !root.deckMode

                label: qsTr("Practice")
                filled: false

                onClicked: root.practiceRequested()
            }

            MobileWideButton
            {
                objectName: "vocabulary_add_word"
                visible: !root.deckMode

                label: qsTr("Add word")

                onClicked: root.addWordRequested()
            }
        }

        MobileSheetLoader
        {
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            width: shell.width
            z: 10
            open: root.filterSheetOpen

            sourceComponent: MobileFilterSheet
            {
                model: root.filterOptions
                selectedKeys: controller.filterKeys

                onDismissed: root.filterSheetOpen = false
                onToggled: function(key) { root.toggleFilterKey(key) }
                onCleared: controller.filterKeys = []
            }
        }

        MobileSheetLoader
        {
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            width: shell.width
            z: 10
            open: root.sortSheetOpen

            sourceComponent: MobileSortSheet
            {
                model: root.sortOptions
                currentKey: controller.sortKey
                descending: controller.sortDescending

                onDismissed: root.sortSheetOpen = false

                onChosen: function(key, desc)
                {
                    controller.sortKey = key
                    controller.sortDescending = desc
                }
            }
        }
    }
}
