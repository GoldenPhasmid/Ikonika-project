import QtQuick
import QtQuick.Dialogs
import QtQuick.Layouts
import "../Decks.js" as Decks

Item
{
    id: root

    required property DictionaryController cardDictionary
    required property var store
    required property VocabularyController vocabulary

    property var deckId: 0
    property string userDeckId: ""
    readonly property bool deckOpen: root.deckId > 0 || root.userDeckId.length > 0
    readonly property var deckVocabulary: deckLoader.item ? deckLoader.item.vocabulary : null
    readonly property string deckName: root.deckVocabulary ? root.deckVocabulary.deckName : ""

    signal dictionaryRequested(string word, string partOfSpeech)
    signal removeCardRequested(string cardId, string word)
    signal cardRemoved(string cardId)
    signal discoverRequested(var deckId, Item source)
    signal learnRequested(string userDeckId)
    signal practiceRequested(string userDeckId)
    signal addCardRequested()

    readonly property bool openDeckBuiltin: root.deckId > 0
    readonly property var shownDeck: (root.openDeckBuiltin
        ? DecksController.decks.find(deck => Number(deck.id) === Number(root.deckId))
        : UserDecksController.decks.find(deck => deck.id === root.userDeckId)) ?? ({})
    readonly property string openDeckCategory: root.openDeckBuiltin ? (root.shownDeck.category ?? "")
        : (UserDecksController.categories.find(category => category.id === root.shownDeck.categoryId)?.name ?? "")
    readonly property color openDeckTint: root.openDeckBuiltin ? root.categoryTint(root.shownDeck.category ?? "")
                                                              : root.customTint(root.shownDeck.categoryId ?? "")
    readonly property bool openDeckUnsaved: deckLoader.item ? deckLoader.item.unsaved : false

    readonly property int openDeckNewCount: (root.openDeckBuiltin ? root.shownDeck.untriagedCount : root.shownDeck.newCount) ?? 0

    function refreshCounts()
    {
        if (root.openDeckBuiltin)
        {
            DecksController.refresh()

            return
        }

        UserDecksController.refresh()
    }

    function handleReturn(event)
    {
        if (!deckLoader.item || deckLoader.item.typing || root.openDeckUnsaved || root.openDeckNewCount === 0)
        {
            event.accepted = false

            return
        }

        root.learnOpenDeck()
    }

    function learnOpenDeck()
    {
        if (root.openDeckBuiltin)
        {
            root.discoverRequested(root.deckId, deckLoader.item.deckHeaderItem)

            return
        }

        root.learnRequested(root.userDeckId)
    }

    function askEditDeck(deck)
    {
        deckDialog.openFor(deck, function(name, description)
        {
            if (UserDecksController.editDeck(deck.id, name, description) && root.userDeckId === deck.id && deckLoader.item)
            {
                deckLoader.item.refresh()
            }
        })
    }

    signal messageRequested(string message)

    function deckFace(deckId)
    {
        return deckLoader.item && Number(root.deckId) === deckId ? deckLoader.item.deckHeaderItem : null
    }

    function refresh()
    {
        DecksController.refresh()
        UserDecksController.refresh()

        if (deckLoader.item)
        {
            deckLoader.item.refresh()
        }
    }

    function focusSearch()
    {
        deckSearch.forceActiveFocus()
        deckSearch.selectAll()
    }

    property string pendingWord: ""
    property string pendingPartOfSpeech: ""

    function openDeck(deckId, word, partOfSpeech)
    {
        root.pendingWord = word ?? ""
        root.pendingPartOfSpeech = partOfSpeech ?? ""

        if (root.deckId === deckId && deckLoader.item)
        {
            deckLoader.item.focusEntry(root.pendingWord, root.pendingPartOfSpeech)
            root.pendingWord = ""

            return
        }

        root.deckId = deckId
    }

    function openUserDeck(userDeckId)
    {
        root.deckId = 0
        root.userDeckId = userDeckId
    }

    function closeDeck()
    {
        root.deckId = 0
        root.userDeckId = ""
    }

    property string section: "all"

    readonly property string categoryPrefix: "category:"
    readonly property string customPrefix: "mine:"
    readonly property bool builtinSection: !root.section.startsWith("mine")
    readonly property string customCategoryId: root.section.startsWith(root.customPrefix) ? root.section.slice(root.customPrefix.length) : ""

    readonly property var customCategories: UserDecksController.categories.map(category => ({
        id: category.id,
        name: category.name,
        count: category.count,
        tint: root.customTint(category.id)
    }))

    readonly property int myDecksCount: UserDecksController.decks.length

    readonly property string sortOrder: AppViewLayouts.revision >= 0 ? AppViewLayouts.sortOrder("userDecks", Decks.sortOrders[0]) : Decks.sortOrders[0]
    readonly property bool sortReversed: AppViewLayouts.revision >= 0 && AppViewLayouts.sortReversed("userDecks")

    readonly property var searchedDecks: Decks.sorted(UserDecksController.decks.filter(deck => Decks.matches(deck, deckSearch.term)),
                                                      root.sortOrder, root.sortReversed)

    readonly property var customDecks: root.searchedDecks.filter(deck => deck.categoryId === root.customCategoryId)

    readonly property var customGroups: root.customCategoryId.length > 0 || root.builtinSection ? []
        : UserDecksController.categories.map(category => ({
              id: category.id,
              name: category.name,
              tint: root.customTint(category.id),
              decks: root.searchedDecks.filter(deck => deck.categoryId === category.id)
          })).filter(group => group.decks.length > 0)

    function askDeleteDeck(deckId, name)
    {
        deleteDialog.ask(qsTr("Delete the deck \"%1\"?").arg(name),
                         qsTr("Its cards stay in My Vocabulary."),
                         qsTr("Delete"),
                         function()
                         {
                             if (root.userDeckId === deckId)
                             {
                                 root.closeDeck()
                             }

                             UserDecksController.deleteDeck(deckId)
                         })
    }

    function customTint(categoryId)
    {
        return Decks.customTint(UserDecksController.categories, categoryId, AppTheme.coverColours, AppTheme.colourCoverMuted)
    }
    readonly property string category: root.section.startsWith(root.categoryPrefix) ? root.section.slice(root.categoryPrefix.length) : ""

    readonly property var categories: Decks.categories(DecksController.decks)

    readonly property var builtinCategories: root.categories.map(name => ({
        name: name,
        count: root.countDecks(name),
        tint: root.categoryTint(name)
    }))

    readonly property var sections:
    {
        if (root.categories.length === 0)
        {
            return [""]
        }

        return root.category.length === 0 ? root.categories : [root.category]
    }

    readonly property string sectionTitle:
    {
        if (root.section === "all")
        {
            return qsTr("All built-in decks")
        }

        if (root.section === "mine")
        {
            return qsTr("My Decks")
        }

        const custom = root.customCategories.find(entry => root.customPrefix + entry.id === root.section)

        return custom ? custom.name : root.category
    }

    readonly property string sectionSummary:
    {
        if (root.section === "all")
        {
            return Decks.summarise(DecksController.decks, Qt.locale())
        }

        if (root.section === "mine")
        {
            return root.searchedDecks.length > 0 ? Decks.summarise(root.searchedDecks, Qt.locale()) : ""
        }

        if (!root.builtinSection)
        {
            return root.customDecks.length > 0 ? Decks.summarise(root.customDecks, Qt.locale()) : ""
        }

        return root.category.length > 0 ? root.categorySummary(root.category) : ""
    }

    readonly property int tileColumns: Math.max(1, Math.floor((page.columnWidth + AppMetrics.gapMedium)
                                                              / (AppMetrics.coverTileMinimumWidth + AppMetrics.gapMedium)))
    readonly property real tileWidth: Math.floor((page.columnWidth - AppMetrics.gapMedium * (root.tileColumns - 1))
                                                 / root.tileColumns)

    function categoryTint(category)
    {
        return Decks.tint(root.categories, category, AppTheme.coverColours)
    }

    function categorySummary(category)
    {
        return Decks.summary(DecksController.decks, category, Qt.locale())
    }

    function countDecks(category)
    {
        return DecksController.decks.filter(deck => deck.category === category).length
    }

    onCategoriesChanged:
    {
        if (root.category.length > 0 && root.categories.indexOf(root.category) < 0)
        {
            root.section = "all"
        }
    }

    Connections
    {
        target: UserDecksController

        function onChanged()
        {
            if (root.customCategoryId.length > 0 && !root.customCategories.some(entry => entry.id === root.customCategoryId))
            {
                root.section = "mine"
            }
        }
    }

    Item
    {
        anchors.fill: parent
        visible: !root.deckOpen

        DesktopDecksPane
        {
            id: decksPane

            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom

            section: root.section
            builtinCategories: root.builtinCategories
            builtinCount: DecksController.decks.length
            customCategories: root.customCategories
            myDecksCount: root.myDecksCount

            onSectionChosen: function(section) { root.section = section }
            onNewCategoryRequested: categoryDialog.create(function(name, colour) { UserDecksController.createCategory(name, colour) })
            onRenameCategoryRequested: function(categoryId, name)
            {
                categoryDialog.rename(categoryId, name, function(newName) { UserDecksController.renameCategory(categoryId, newName) })
            }
            onDeleteCategoryRequested: function(categoryId, name)
            {
                deleteDialog.ask(qsTr("Delete the category \"%1\"?").arg(name),
                                 qsTr("Its decks move to My Decks."),
                                 qsTr("Delete"),
                                 function() { UserDecksController.deleteCategory(categoryId) })
            }
            onImportRequested: importDialog.open()
        }

        ColumnLayout
        {
            anchors.left: decksPane.right
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            spacing: 0

            Item
            {
                Layout.fillWidth: true
                Layout.preferredHeight: toolbar.height + AppMetrics.gapMedium

                RowLayout
                {
                    id: toolbar

                    x: (parent.width - width) / 2
                    y: AppMetrics.gapMedium
                    width: page.columnWidth
                    spacing: AppMetrics.gapSmall

                    DesktopLabel
                    {
                        Layout.minimumWidth: implicitWidth
                        text: root.sectionTitle
                        font.pixelSize: AppMetrics.fontTitle
                        font.weight: Font.DemiBold
                    }

                    Item
                    {
                        Layout.fillWidth: true
                    }

                    DesktopLabel
                    {
                        visible: root.sectionSummary.length > 0
                        text: root.sectionSummary
                        font.pixelSize: AppMetrics.fontSmall
                        color: AppTheme.colourMuted
                    }

                    DesktopButton
                    {
                        id: sortButton

                        objectName: "deck_sort"

                        visible: !root.builtinSection
                        iconName: root.sortReversed ? "arrow_up_narrow_wide" : "arrow_down_wide_narrow"
                        trailingIconName: "chevron_down"
                        text: Decks.sortLabels()[Decks.sortOrders.indexOf(root.sortOrder)]

                        onClicked: sortPopover.visible ? sortPopover.close() : sortPopover.open()

                        DesktopPopover
                        {
                            id: sortPopover

                            x: sortButton.width - width
                            y: sortButton.height + AppMetrics.gapTight
                            heading: qsTr("Sort by")

                            Repeater
                            {
                                model: Decks.sortOrders

                                delegate: DesktopPopoverRow
                                {
                                    required property string modelData
                                    required property int index

                                    objectName: "deck_sort_" + modelData
                                    Layout.fillWidth: true
                                    text: Decks.sortLabels()[index]
                                    showsCheck: true
                                    selected: root.sortOrder === modelData

                                    onClicked: AppViewLayouts.setSort("userDecks", modelData, root.sortReversed)
                                }
                            }

                            DesktopDivider
                            {
                                Layout.fillWidth: true
                                Layout.topMargin: AppMetrics.gapTight
                                Layout.bottomMargin: AppMetrics.gapTight
                            }

                            Repeater
                            {
                                model: 2

                                delegate: DesktopPopoverRow
                                {
                                    required property int index

                                    objectName: "deck_sort_direction_" + index
                                    Layout.fillWidth: true
                                    text: Decks.sortDirections()[root.sortOrder][index]
                                    showsCheck: true
                                    selected: root.sortReversed === (index === 1)

                                    onClicked: AppViewLayouts.setSort("userDecks", root.sortOrder, index === 1)
                                }
                            }
                        }
                    }

                    DesktopSearchField
                    {
                        id: deckSearch

                        objectName: "deck_search"

                        Layout.fillWidth: true
                        Layout.preferredWidth: AppMetrics.searchWidth
                        Layout.maximumWidth: AppMetrics.searchWidth
                        placeholderText: qsTr("Search deck")
                    }
                }
            }

            DesktopPage
            {
                id: page

                Layout.fillWidth: true
                Layout.fillHeight: true
                maximumWidth: AppMetrics.coverGridWidth
                topSpacing: AppMetrics.gapMedium
                spacing: AppMetrics.gapMedium

                Flow
                {
                    Layout.fillWidth: true
                    visible: !root.builtinSection
                    spacing: AppMetrics.gapMedium

                    Accessible.role: Accessible.List
                    Accessible.name: root.sectionTitle

                    DesktopNewDeckTile
                    {
                        objectName: "deck_new"

                        width: root.tileWidth

                        onClicked:
                        {
                            const categoryId = root.customCategoryId
                            deckDialog.openFor(null, function(name, description)
                            {
                                UserDecksController.createDeck(name, categoryId, description)
                            })
                        }
                    }

                    Repeater
                    {
                        model: root.customDecks

                        delegate: DesktopCustomDeckTile
                        {
                            required property var modelData

                            deck: modelData
                            width: root.tileWidth
                            categoryTint: root.customTint(modelData.categoryId)

                            onOpenRequested: root.openUserDeck(modelData.id)
                            onEditRequested: root.askEditDeck(modelData)
                            onDeleteRequested: function(deckId, name) { root.askDeleteDeck(deckId, name) }
                        }
                    }
                }

                Repeater
                {
                    model: root.customGroups

                    delegate: ColumnLayout
                    {
                        id: customGroup

                        required property var modelData

                        Layout.fillWidth: true
                        spacing: AppMetrics.gapSmall

                        RowLayout
                        {
                            Layout.fillWidth: true
                            spacing: AppMetrics.gapMedium

                            CategoryDot
                            {
                                colour: customGroup.modelData.tint
                            }

                            DesktopSectionLabel
                            {
                                text: customGroup.modelData.name
                            }

                            DesktopDivider
                            {
                                Layout.fillWidth: true
                            }

                            DesktopLabel
                            {
                                text: Decks.summarise(customGroup.modelData.decks, Qt.locale())
                                font.pixelSize: AppMetrics.fontSmall
                                color: AppTheme.colourMuted
                            }
                        }

                        Flow
                        {
                            Layout.fillWidth: true
                            spacing: AppMetrics.gapMedium

                            Accessible.role: Accessible.List
                            Accessible.name: customGroup.modelData.name

                            Repeater
                            {
                                model: customGroup.modelData.decks

                                delegate: DesktopCustomDeckTile
                                {
                                    required property var modelData

                                    deck: modelData
                                    width: root.tileWidth
                                    categoryTint: customGroup.modelData.tint

                                    onOpenRequested: root.openUserDeck(modelData.id)
                                    onEditRequested: root.askEditDeck(modelData)
                                    onDeleteRequested: function(deckId, name) { root.askDeleteDeck(deckId, name) }
                                }
                            }
                        }
                    }
                }

                Repeater
                {
                    model: root.builtinSection ? root.sections : []

                    delegate: ColumnLayout
                    {
                        id: section

                        required property string modelData
                        required property int index

                        Layout.fillWidth: true
                        Layout.topMargin: section.index > 0 ? AppMetrics.gapMedium : 0
                        spacing: AppMetrics.gapSmall

                        visible: deckRepeater.count > 0

                        RowLayout
                        {
                            Layout.fillWidth: true
                            visible: section.modelData.length > 0 && root.category.length === 0
                            spacing: AppMetrics.gapMedium

                            CategoryDot
                            {
                                colour: root.categoryTint(section.modelData)
                            }

                            DesktopSectionLabel
                            {
                                text: section.modelData
                            }

                            DesktopDivider
                            {
                                Layout.fillWidth: true
                            }

                            DesktopLabel
                            {
                                text: root.categorySummary(section.modelData)
                                font.pixelSize: AppMetrics.fontSmall
                                color: AppTheme.colourMuted
                            }
                        }

                        Flow
                        {
                            Layout.fillWidth: true
                            spacing: AppMetrics.gapMedium

                            Accessible.role: Accessible.List
                            Accessible.name: section.modelData.length > 0 ? section.modelData : qsTr("Decks")

                            Repeater
                            {
                                id: deckRepeater

                                model: DecksController.decks.filter(deck => deck.category === section.modelData
                                                                            && Decks.matches(deck, deckSearch.term))

                                delegate: DesktopDeckTile
                                {
                                    required property var modelData

                                    objectName: "deck_" + modelData.id

                                    width: root.tileWidth
                                    title: modelData.name
                                    iconName: modelData.icon
                                    tint: root.categoryTint(modelData.category)
                                    wordCount: modelData.wordCount
                                    learningCount: modelData.learningCount
                                    learnedCount: modelData.learnedCount

                                    onOpenRequested: root.openDeck(modelData.id)
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    DesktopCollectionDialog
    {
        id: categoryDialog

        newTitle: qsTr("New category")
        renameTitle: qsTr("Rename category")
    }

    DesktopDeckDialog
    {
        id: deckDialog
    }

    DesktopConfirmDialog
    {
        id: deleteDialog
    }

    FileDialog
    {
        id: importDialog

        title: qsTr("Choose a file to import")
        nameFilters: [qsTr("Word lists (*.txt *.csv *.anki2)")]

        onAccepted:
        {
            const result = UserDecksController.importFromFile(importDialog.selectedFile, root.customCategoryId)

            if (!result.ok)
            {
                root.messageRequested(qsTr("No words could be imported from that file."))

                return
            }

            root.section = root.customCategoryId.length > 0 ? root.section : "mine"
            root.messageRequested(Decks.importMessage(result))
        }
    }

    FileDialog
    {
        id: importIntoDialog

        title: qsTr("Choose a file to import")
        nameFilters: importDialog.nameFilters

        onAccepted:
        {
            const result = UserDecksController.importIntoDeck(importIntoDialog.selectedFile, root.userDeckId)

            if (!result.ok)
            {
                root.messageRequested(qsTr("No words could be imported from that file."))

                return
            }

            if (deckLoader.item)
            {
                deckLoader.item.refresh()
            }

            root.messageRequested(Decks.importMessage(result))
        }
    }

    FileDialog
    {
        id: exportDialog

        title: qsTr("Export deck")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "csv"
        nameFilters: [qsTr("Word lists (*.csv)")]

        onAccepted:
        {
            root.messageRequested(root.vocabulary.exportToFile(exportDialog.selectedFile)
                                  ? qsTr("Exported %1").arg(root.deckName) : qsTr("The deck could not be exported."))
        }
    }

    Component
    {
        id: headerComponent

        DesktopDeckHeader
        {
            deck: root.shownDeck
            builtin: root.openDeckBuiltin
            compact: root.height < AppMetrics.deckHeaderRoomyHeight
            editing: root.openDeckUnsaved
            categoryName: root.openDeckCategory
            tint: root.openDeckTint

            onLearnRequested: root.learnOpenDeck()
            onPracticeRequested: root.practiceRequested(root.userDeckId)
            onImportRequested: importIntoDialog.open()
            onExportRequested:
            {
                exportDialog.selectedFile = "file:///" + encodeURIComponent(root.deckName.replace(/[\\/:*?"<>|]/g, " ").trim()) + ".csv"
                exportDialog.open()
            }
            onEditRequested: root.askEditDeck(root.shownDeck)
            onDeleteRequested: root.askDeleteDeck(root.userDeckId, root.deckName)
            onCategoryChosen: function(categoryId) { UserDecksController.moveDeck(root.userDeckId, categoryId) }
            onDailyLearningChosen: function(dailyLearning) { UserDecksController.setDailyLearning(root.userDeckId, dailyLearning) }
        }
    }

    Connections
    {
        target: root.deckVocabulary

        function onVocabularyChanged()
        {
            Qt.callLater(root.refreshCounts)
        }
    }

    Keys.onReturnPressed: function(event) { root.handleReturn(event) }
    Keys.onEnterPressed: function(event) { root.handleReturn(event) }

    Loader
    {
        id: deckLoader

        anchors.fill: parent
        active: root.deckOpen

        onLoaded:
        {
            deckLoader.item.forceActiveFocus()
            deckLoader.item.resetView()
            deckLoader.item.focusEntry(root.pendingWord, root.pendingPartOfSpeech)
            root.pendingWord = ""
        }

        sourceComponent: DesktopVocabularyScreen
        {
            controller: root.vocabulary
            cardDictionary: root.cardDictionary
            store: root.store
            deckId: root.deckId
            userDeckId: root.userDeckId

            deckHeader: headerComponent

            onAddCardRequested: root.addCardRequested()
            onDictionaryRequested: function(word, partOfSpeech) { root.dictionaryRequested(word, partOfSpeech) }
            onRemoveCardRequested: function(cardId, word) { root.removeCardRequested(cardId, word) }
            onCardRemoved: function(cardId) { root.cardRemoved(cardId) }
        }
    }
}
