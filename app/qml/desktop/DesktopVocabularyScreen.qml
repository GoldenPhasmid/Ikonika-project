import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item
{
    id: root

    required property var store

    property var deckId: 0
    readonly property bool deckMode: root.deckId > 0

    readonly property alias vocabulary: controller

    property int selectedIndex: -1

    readonly property var selectedCard: root.selectedIndex >= 0 && root.selectedIndex < controller.visibleCards.length
        ? controller.visibleCards[root.selectedIndex] : null
    readonly property bool selectedInVocabulary: root.selectedCard !== null && root.selectedCard.inVocabulary

    signal dictionaryRequested(string word, string partOfSpeech)
    signal cardRemoved(string cardId)
    signal removeCardRequested(string cardId, string word)

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
            if (option.key === controller.sortKey)
            {
                return controller.sortDescending ? option.inverted : option.primary
            }
        }

        return qsTr("Sort")
    }

    function resetView()
    {
        controller.resetView()
        searchField.text = ""
        root.selectedIndex = controller.visibleCards.length > 0 ? 0 : -1
        root.showSelected()
    }

    function refresh()
    {
        const keptId = root.selectedCard ? root.selectedCard.id : ""
        const scrollPosition = list.contentY

        controller.refresh()

        root.reselect(keptId)
        root.showSelected(true)
        Qt.callLater(function() { list.contentY = scrollPosition })
    }

    function reselect(cardId)
    {
        const found = controller.visibleCards.findIndex(Card => Card.id === cardId)

        root.selectedIndex = found >= 0 ? found : Math.min(root.selectedIndex, controller.visibleCards.length - 1)
        root.showSelected()
    }

    function showSelected(force)
    {
        pane.show(root.selectedCard, force === true)
    }

    function select(index)
    {
        if (index < 0 || index >= controller.visibleCards.length)
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
        if (controller.removeCard(cardId))
        {
            root.cardRemoved(cardId)
            root.reselect("")
        }
    }

    VocabularyController
    {
        id: controller

        deckId: root.deckId

        onViewChanged: Qt.callLater(function()
        {
            if (root.selectedIndex >= controller.visibleCards.length)
            {
                root.selectedIndex = controller.visibleCards.length - 1
            }

            if (root.selectedIndex < 0 && controller.visibleCards.length > 0)
            {
                root.selectedIndex = 0
            }

            root.showSelected()
        })
    }

    DesktopSidePane
    {
        id: listPane

        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        widthKey: "vocabularyList"
        defaultWidth: AppMetrics.listPaneWidth

        DesktopSearchField
        {
            id: searchField

            objectName: "vocabulary_search"

            Layout.fillWidth: true
            placeholderText: root.deckMode ? qsTr("Search %1").arg(controller.deckName) : qsTr("Search My Vocabulary")

            onTermChanged: controller.search = searchField.text + searchField.preeditText

            Keys.onDownPressed: root.select(root.selectedIndex + 1)
            Keys.onUpPressed: root.select(root.selectedIndex - 1)
            Keys.onReturnPressed: if (root.selectedCard) root.editCard(root.selectedIndex)

            Connections
            {
                target: controller

                function onVocabularyChanged() { searchField.text = controller.search }
            }
        }

        MobileVocabularyBar
        {
            readonly property string selectedFilterKey: controller.filterKeys.length === 1 ? controller.filterKeys[0] : ""

            notStartedCount: controller.notLearnedCount
            learningCount: controller.learningCount
            learnedCount: controller.learnedCount
            selectedKey: selectedFilterKey

            onStageOpened: function(filterKey)
            {
                controller.filterKeys = filterKey === selectedFilterKey ? [] : [filterKey]
            }
        }

        RowLayout
        {
            Layout.fillWidth: true
            spacing: AppMetrics.gapSmall

            DesktopLabel
            {
                Layout.fillWidth: true
                text: qsTr("%n words", "", controller.filteredCount)
                font.pixelSize: AppMetrics.fontSmall
                color: AppTheme.colourMuted
            }

            DesktopButton
            {
                id: sortButton

                tone: "ghost"
                text: root.sortLabel
                trailingIconName: "chevron_down"

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
                                selected: controller.sortKey === modelData.key && !controller.sortDescending

                                onClicked:
                                {
                                    controller.sortKey = modelData.key
                                    controller.sortDescending = false
                                    sortPopover.close()
                                }
                            }

                            DesktopPopoverRow
                            {
                                text: modelData.inverted
                                showsCheck: true
                                selected: controller.sortKey === modelData.key && controller.sortDescending

                                onClicked:
                                {
                                    controller.sortKey = modelData.key
                                    controller.sortDescending = true
                                    sortPopover.close()
                                }
                            }
                        }
                    }
                }
            }
        }

        DesktopWordList
        {
            id: list

            objectName: "vocabulary_words"

            emptyText: searchField.text.length > 0 ? qsTr("No words match.") : qsTr("No words in this view.")

            Accessible.name: qsTr("Words in My Vocabulary")

            model: controller.visibleCards

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

                canEdit: modelData.inVocabulary
                canComplete: modelData.inVocabulary && !modelData.completed
                canReset: modelData.inVocabulary && (modelData.box > 0 || modelData.completed)
                canRemove: modelData.inVocabulary

                onClicked: root.select(index)
                onDoubleClicked: root.editCard(index)
                onEditRequested: root.editCard(index)
                onCompleteRequested: controller.completeCardProgress(modelData.id)
                onResetRequested: controller.resetCardProgress(modelData.id)
                onRemoveRequested: root.removeCardRequested(modelData.id, modelData.targetWord)
            }
        }
    }

    DesktopCardPane
    {
        id: pane

        anchors.left: listPane.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom

        controller: controller
        card: root.selectedCard
        stageCount: root.store.stageCount
        emptyText: controller.visibleCards.length === 0 ? qsTr("Add words to My Vocabulary from Words.")
                                                         : ""

        onSaved: root.refresh()
        onRemoveRequested: function(cardId, word) { root.removeCardRequested(cardId, word) }
        onDictionaryRequested: function(word, partOfSpeech) { root.dictionaryRequested(word, partOfSpeech) }
    }
}
