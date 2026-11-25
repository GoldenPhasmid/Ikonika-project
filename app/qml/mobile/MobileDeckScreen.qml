import QtQuick
import QtQuick.Dialogs
import "../Decks.js" as Decks

Item
{
    id: root

    required property var store
    required property VocabularyController controller
    required property DictionaryController cardDictionary

    property bool splitLayout: false
    property var deckId: 0
    property string userDeckId: ""
    property bool deckActionsOpen: false
    property bool cardActionsOpen: false
    property var afterDeckActions: null

    readonly property var vocabulary: root.controller
    readonly property bool builtin: root.deckId > 0
    readonly property var shownDeck: (root.builtin
        ? DecksController.decks.find(deck => Number(deck.id) === Number(root.deckId))
        : UserDecksController.decks.find(deck => deck.id === root.userDeckId)) ?? ({})
    readonly property string categoryName: root.builtin ? (root.shownDeck.category ?? "")
        : (UserDecksController.categories.find(category => category.id === root.shownDeck.categoryId)?.name ?? "")
    readonly property color tint: root.builtin
        ? Decks.tint(Decks.categories(DecksController.decks), root.shownDeck.category ?? "", AppTheme.coverColours)
        : Decks.customTint(UserDecksController.categories, root.shownDeck.categoryId ?? "", AppTheme.coverColours,
                           AppTheme.colourCoverMuted)

    signal backRequested()
    signal tabSelected(string key)
    signal discoverRequested(var deckId)
    signal learnRequested(string userDeckId)
    signal practiceRequested(string userDeckId)
    signal addCardRequested()
    signal searchRequested()
    signal wordOpened(string word, string partOfSpeech)
    signal editWordRequested(string cardId, string word, string translation, int gender, string partOfSpeech, int box)
    signal cardRemoved(string cardId)
    signal dictionaryRequested(string word, string partOfSpeech)
    signal messageRequested(string message)
    signal deckDeleted()

    function resetView()
    {
        listScreen.resetView()

        if (split.item)
        {
            split.item.screen.resetView()
        }
    }

    function focusEntry(word)
    {
        listScreen.focusEntry(word)

        if (split.item)
        {
            split.item.screen.focusEntry(word, "")
        }
    }

    function refresh()
    {
        listScreen.refresh()

        if (split.item)
        {
            split.item.screen.refresh()
        }
    }

    function reselect(cardId)
    {
        split.active = true
        split.item.screen.reselect(cardId)
        split.item.screen.select(split.item.screen.selectedIndex)
    }

    function dismissSheet()
    {
        if (levelPicker.visible)
        {
            levelPicker.close()

            return true
        }

        if (nameSheet.visible)
        {
            nameSheet.close()

            return true
        }

        if (root.deckActionsOpen || root.cardActionsOpen)
        {
            root.deckActionsOpen = false
            root.cardActionsOpen = false

            return true
        }

        return false
    }

    function refreshCounts()
    {
        if (root.builtin)
        {
            DecksController.refresh()

            return
        }

        UserDecksController.refresh()
    }

    function learn()
    {
        if (root.builtin)
        {
            root.discoverRequested(root.deckId)

            return
        }

        root.learnRequested(root.userDeckId)
    }

    function editDeck()
    {
        const deckId = root.userDeckId

        nameSheet.openDeck(qsTr("Edit deck"), root.shownDeck.name ?? "", root.shownDeck.description ?? "",
                           function(name, description)
                           {
                               if (UserDecksController.editDeck(deckId, name, description))
                               {
                                   root.refresh()
                               }
                           })
    }

    function chooseCategory()
    {
        const deckId = root.userDeckId
        const targets = [{ id: "", name: qsTr("No category") }].concat(UserDecksController.categories)

        levelPicker.open(qsTr("Category"), targets.map(category => category.name),
                         targets.findIndex(category => category.id === (root.shownDeck.categoryId ?? "")),
                         function(index) { UserDecksController.moveDeck(deckId, targets[index].id) })
    }

    function exportDeck()
    {
        exportDialog.selectedFile = "file:///" + encodeURIComponent(root.controller.deckName.replace(/[\\/:*?"<>|]/g, " ").trim()) + ".csv"
        exportDialog.open()
    }

    function deleteDeck()
    {
        console.info("Deck screen deletes deck", root.userDeckId)

        if (UserDecksController.deleteDeck(root.userDeckId))
        {
            root.deckDeleted()
        }
    }

    Connections
    {
        target: root.controller

        function onVocabularyChanged()
        {
            Qt.callLater(root.refreshCounts)
        }
    }

    Component
    {
        id: headerComponent

        DesktopDeckHeader
        {
            deck: root.shownDeck
            builtin: root.builtin
            compact: !root.splitLayout || root.height < AppMetrics.deckHeaderRoomyHeight
            sheetActions: true
            categoryName: root.categoryName
            tint: root.tint

            onLearnRequested: root.learn()
            onPracticeRequested: root.practiceRequested(root.userDeckId)
            onEditRequested: root.editDeck()
        }
    }

    MobileVocabularyScreen
    {
        id: listScreen

        anchors.fill: parent
        visible: !root.splitLayout

        controller: root.controller
        store: root.store
        deckId: root.deckId
        userDeckId: root.userDeckId
        deckHeader: headerComponent

        onBackRequested: root.backRequested()
        onOptionsRequested: root.deckActionsOpen = true
        onSearchRequested: root.searchRequested()
        onAddWordRequested: root.addCardRequested()
        onWordOpened: function(word, partOfSpeech) { root.wordOpened(word, partOfSpeech) }
        onEditWordRequested: function(cardId, word, translation, gender, partOfSpeech, box)
        {
            root.editWordRequested(cardId, word, translation, gender, partOfSpeech, box)
        }
        onCardRemoved: function(cardId) { root.cardRemoved(cardId) }
        onTabSelected: function(key) { root.tabSelected(key) }
    }

    Component
    {
        id: splitComponent

        MobileShell
        {
            readonly property alias screen: splitScreen

            activeTab: "decks"
            contentMaximumWidth: Number.POSITIVE_INFINITY
            headerTitle: root.controller.deckName
            showBack: true
            showOptions: true
            showTabBar: true

            onBackRequested: root.backRequested()
            onOptionsRequested: root.deckActionsOpen = true
            onTabSelected: function(key) { root.tabSelected(key) }

            DesktopVocabularyScreen
            {
                id: splitScreen

                anchors.fill: parent

                cardDictionary: root.cardDictionary
                controller: root.controller
                store: root.store
                deckId: root.deckId
                userDeckId: root.userDeckId
                deckHeader: headerComponent
                compact: true

                onAddCardRequested: root.addCardRequested()
                onDictionaryRequested: function(word, partOfSpeech) { root.dictionaryRequested(word, partOfSpeech) }
                onRemoveCardRequested: function(cardId, word) { splitScreen.removeCard(cardId) }
                onCardRemoved: function(cardId) { root.cardRemoved(cardId) }
                onCardActionsRequested: root.cardActionsOpen = true
            }
        }
    }

    Loader
    {
        id: split

        anchors.fill: parent
        visible: root.splitLayout
        active: false
        asynchronous: false
        sourceComponent: splitComponent

        onVisibleChanged: if (visible) active = true

        onLoaded: split.item.screen.resetView()
    }

    MobileSheetLoader
    {
        anchors.fill: parent
        z: 10
        open: root.deckActionsOpen

        sourceComponent: MobileDeckActionsSheet
        {
            deck: root.shownDeck
            builtin: root.builtin
            categoryName: root.categoryName

            onDismissed:
            {
                root.deckActionsOpen = false

                const action = root.afterDeckActions

                root.afterDeckActions = null

                if (action)
                {
                    action()
                }
            }
            onEditRequested: root.afterDeckActions = root.editDeck
            onCategoryRequested: root.afterDeckActions = root.chooseCategory
            onDailyLearningChosen: function(dailyLearning) { UserDecksController.setDailyLearning(root.userDeckId, dailyLearning) }
            onImportRequested: importDialog.open()
            onExportRequested: root.exportDeck()
            onDeleteRequested: root.deleteDeck()
        }
    }

    MobileSheetLoader
    {
        anchors.fill: parent
        z: 10
        open: root.cardActionsOpen

        sourceComponent: MobileCardActionsSheet
        {
            pane: split.item.screen.pane

            onDismissed: root.cardActionsOpen = false
        }
    }

    MobileSettingsPicker
    {
        id: levelPicker

        anchors.fill: parent
        z: 60
    }

    MobileNameSheet
    {
        id: nameSheet

        z: 55
    }

    FileDialog
    {
        id: importDialog

        title: qsTr("Choose a file to import")
        nameFilters: [qsTr("Word lists (*.txt *.csv *.anki2)")]

        onAccepted:
        {
            const result = UserDecksController.importIntoDeck(importDialog.selectedFile, root.userDeckId)

            if (!result.ok)
            {
                root.messageRequested(qsTr("No words could be imported from that file."))

                return
            }

            root.refresh()
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
            root.messageRequested(root.controller.exportToFile(exportDialog.selectedFile)
                                  ? qsTr("Exported %1").arg(root.controller.deckName) : qsTr("The deck could not be exported."))
        }
    }
}
