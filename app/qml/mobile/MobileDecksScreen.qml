import QtQuick
import QtQuick.Dialogs
import QtQuick.Layouts
import "../Decks.js" as Decks

Item
{
    id: root

    signal deckOpened(var deckId)
    signal userDeckOpened(string deckId)
    signal tabSelected(string key)
    signal messageRequested(string message)

    property bool searchOpen: false
    property var afterPicker: null
    property string section: "all"

    readonly property string categoryPrefix: "category:"
    readonly property string customPrefix: "mine:"
    readonly property bool builtinSection: !root.section.startsWith("mine")
    readonly property string customCategoryId: root.section.startsWith(root.customPrefix) ? root.section.slice(root.customPrefix.length) : ""
    readonly property string category: root.section.startsWith(root.categoryPrefix) ? root.section.slice(root.categoryPrefix.length) : ""

    readonly property var categories: Decks.categories(DecksController.decks)
    readonly property bool listLayout: AppViewLayouts.revision >= 0 && AppViewLayouts.isList("decks")
    readonly property string searchTerm: (decksShell.searchFieldText + decksShell.searchFieldPreeditText).trim()

    readonly property var builtinCategories: root.categories.map(name => ({
        name: name,
        count: DecksController.decks.filter(deck => deck.category === name).length,
        tint: root.categoryTint(name)
    }))

    readonly property var customCategories: UserDecksController.categories.map(category => ({
        id: category.id,
        name: category.name,
        count: category.count,
        tint: root.customTint(category.id)
    }))

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

    readonly property string sortOrder: AppViewLayouts.revision >= 0 ? AppViewLayouts.sortOrder("userDecks", Decks.sortOrders[0]) : Decks.sortOrders[0]
    readonly property bool sortReversed: AppViewLayouts.revision >= 0 && AppViewLayouts.sortReversed("userDecks")

    readonly property var groups:
    {
        const term = root.searchTerm

        if (root.section === "mine" || root.customCategoryId.length > 0)
        {
            const decks = Decks.sorted(UserDecksController.decks.filter(deck => Decks.matches(deck, term)),
                                       root.sortOrder, root.sortReversed)
            const list = [{
                name: "",
                tint: AppTheme.colourCoverMuted,
                headed: false,
                custom: true,
                showNew: true,
                decks: decks.filter(deck => deck.categoryId === root.customCategoryId)
            }]

            if (root.section === "mine")
            {
                for (const category of root.customCategories)
                {
                    list.push({
                        name: category.name,
                        tint: category.tint,
                        headed: true,
                        custom: true,
                        showNew: false,
                        decks: decks.filter(deck => deck.categoryId === category.id)
                    })
                }
            }

            return list.filter(group => group.showNew || group.decks.length > 0)
        }

        const names = root.category.length > 0 ? [root.category] : root.categories

        return names.map(name => ({
            name: name,
            tint: root.categoryTint(name),
            headed: root.category.length === 0,
            custom: false,
            showNew: false,
            decks: DecksController.decks.filter(deck => deck.category === name && Decks.matches(deck, term))
        })).filter(group => group.decks.length > 0)
    }

    function categoryTint(name)
    {
        return Decks.tint(root.categories, name, AppTheme.coverColours)
    }

    function customTint(categoryId)
    {
        return Decks.customTint(UserDecksController.categories, categoryId, AppTheme.coverColours, AppTheme.colourCoverMuted)
    }

    function refresh()
    {
        DecksController.refresh()
        UserDecksController.refresh()
    }

    function closeSearch()
    {
        root.searchOpen = false
        decksShell.searchFieldText = ""
        root.forceActiveFocus()
    }

    function dismissSheet()
    {
        if (levelPicker.visible)
        {
            levelPicker.close()

            return true
        }

        if (confirmSheet.visible)
        {
            confirmSheet.close()

            return true
        }

        if (nameSheet.visible)
        {
            nameSheet.close()

            return true
        }

        if (sortSheet.visible)
        {
            sortSheet.close()

            return true
        }

        if (sectionsSheet.visible)
        {
            sectionsSheet.close()

            return true
        }

        if (root.searchOpen)
        {
            root.closeSearch()

            return true
        }

        return false
    }

    function chooseSection(section)
    {
        root.section = section
        deckPage.contentY = 0
    }

    function createDeck()
    {
        const categoryId = root.customCategoryId

        nameSheet.openDeck(qsTr("New deck"), "", "",
                           function(name, description) { UserDecksController.createDeck(name, categoryId, description) })
    }

    function createCategory()
    {
        nameSheet.open(qsTr("New category"), "", qsTr("Category name"), true,
                       UserDecksController.categories.length % AppTheme.coverColours.length,
                       function(name, colour) { UserDecksController.createCategory(name, colour) })
    }

    function showCategoryActions(categoryId, name)
    {
        levelPicker.open(name, [qsTr("Rename"), qsTr("Delete category")], -1, function(index)
        {
            if (index === 0)
            {
                root.afterPicker = function()
                {
                    nameSheet.open(qsTr("Rename category"), name, qsTr("Category name"), false, 0,
                                   function(newName) { UserDecksController.renameCategory(categoryId, newName) })
                }

                return
            }

            confirmSheet.ask(qsTr("Delete the category \"%1\"?").arg(name),
                             qsTr("Its decks move to My Decks."),
                             qsTr("Delete"),
                             function() { UserDecksController.deleteCategory(categoryId) })
        })
    }

    function showMoveTargets(deck)
    {
        const targets = UserDecksController.categories.filter(category => category.id !== deck.categoryId)

        levelPicker.open(qsTr("Move to"), targets.map(category => category.name), -1,
                         function(index) { UserDecksController.moveDeck(deck.id, targets[index].id) })
    }

    function showDeckActions(deck)
    {
        const inCategory = deck.categoryId.length > 0
        const movable = UserDecksController.categories.some(category => category.id !== deck.categoryId)
        const actions = [qsTr("Edit deck")]

        if (inCategory)
        {
            actions.push(qsTr("Remove from category"))
        }

        if (movable)
        {
            actions.push(qsTr("Move to…"))
        }

        actions.push(qsTr("Delete deck"))

        levelPicker.open(deck.name, actions, -1, function(index)
        {
            const action = actions[index]

            if (action === qsTr("Edit deck"))
            {
                root.afterPicker = function()
                {
                    nameSheet.openDeck(qsTr("Edit deck"), deck.name, deck.description ?? "",
                                       function(name, description) { UserDecksController.editDeck(deck.id, name, description) })
                }

                return
            }

            if (action === qsTr("Remove from category"))
            {
                UserDecksController.moveDeck(deck.id, "")

                return
            }

            if (action === qsTr("Move to…"))
            {
                Qt.callLater(function() { root.showMoveTargets(deck) })

                return
            }

            confirmSheet.ask(qsTr("Delete the deck \"%1\"?").arg(deck.name),
                             qsTr("Its cards stay in My Vocabulary."),
                             qsTr("Delete"),
                             function() { UserDecksController.deleteDeck(deck.id) })
        })
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

    MobileShell
    {
        id: decksShell

        anchors.fill: parent

        activeTab: "decks"
        headerTitle: root.sectionTitle
        showBack: root.searchOpen
        leadingIcon: "menu"
        showSearchField: root.searchOpen

        onLeadingActivated: sectionsSheet.open()
        onBackRequested: root.closeSearch()
        onSearchFieldAccepted: root.forceActiveFocus()
        onTabSelected: function(key) { root.tabSelected(key) }

        headerStatus: RowLayout
        {
            Layout.fillWidth: false
            spacing: 0

            MobileIconButton
            {
                objectName: "decks_search"

                visible: !root.searchOpen
                iconName: "search"
                text: qsTr("Search decks")

                onClicked:
                {
                    root.searchOpen = true
                    Qt.callLater(decksShell.focusSearchField)
                }
            }

            MobileIconButton
            {
                objectName: "decks_sort"

                visible: !root.searchOpen && !root.builtinSection
                iconName: root.sortReversed ? "arrow_up_narrow_wide" : "arrow_down_wide_narrow"
                text: qsTr("Sort decks")

                onClicked: sortSheet.open()
            }

            MobileIconButton
            {
                objectName: "decks_layout_switch"

                visible: !root.searchOpen
                iconName: root.listLayout ? "layout_grid" : "layout_rows"
                text: root.listLayout ? qsTr("Show as tiles") : qsTr("Show as a list")

                onClicked: AppViewLayouts.setList("decks", !root.listLayout)
            }
        }

        Flickable
        {
            id: deckPage

            anchors.fill: parent
            contentWidth: width
            contentHeight: sectionColumn.implicitHeight + AppMetrics.contentPadding * 2
            boundsBehavior: Flickable.StopAtBounds
            clip: true

            ColumnLayout
            {
                id: sectionColumn

                x: AppMetrics.contentPadding
                y: AppMetrics.contentPadding
                width: deckPage.width - AppMetrics.contentPadding * 2
                spacing: AppMetrics.gapWide

                Text
                {
                    Layout.fillWidth: true
                    visible: root.groups.length === 0
                    text: root.searchTerm.length > 0 ? qsTr("No decks match \"%1\"").arg(root.searchTerm) : qsTr("No decks here yet.")
                    color: AppTheme.colourMuted
                    font.pixelSize: AppMetrics.fontBody
                    font.family: AppMetrics.fontFamily
                    wrapMode: Text.Wrap
                }

                Repeater
                {
                    model: root.groups

                    delegate: ColumnLayout
                    {
                        id: group

                        required property var modelData

                        Layout.fillWidth: true
                        spacing: root.listLayout ? AppMetrics.gapTight : AppMetrics.gapCompact

                        RowLayout
                        {
                            Layout.fillWidth: true
                            visible: group.modelData.headed
                            spacing: AppMetrics.gapSmall

                            CategoryDot
                            {
                                colour: group.modelData.tint
                            }

                            MobileSectionLabel
                            {
                                text: group.modelData.name.toUpperCase()
                            }

                            Rectangle
                            {
                                Layout.fillWidth: true
                                Layout.preferredHeight: AppMetrics.borderWidth
                                color: AppTheme.colourBorder
                            }

                            Text
                            {
                                text: Decks.summarise(group.modelData.decks, Qt.locale())
                                font.pixelSize: AppMetrics.fontMicro
                                font.family: AppMetrics.fontFamily
                                color: AppTheme.colourMuted
                            }
                        }

                        MobileCoverGrid
                        {
                            id: coverGrid

                            listLayout: root.listLayout

                            Accessible.role: Accessible.List
                            Accessible.name: group.modelData.name.length > 0 ? group.modelData.name : root.sectionTitle

                            MobileNewDeckTile
                            {
                                objectName: "deck_new"

                                visible: group.modelData.showNew
                                width: coverGrid.tileWidth
                                compact: root.listLayout

                                onClicked: root.createDeck()
                            }

                            Repeater
                            {
                                model: group.modelData.decks

                                delegate: MobileDeck
                                {
                                    required property var modelData

                                    objectName: (group.modelData.custom ? "user_deck_" : "deck_") + modelData.id

                                    width: coverGrid.tileWidth
                                    compact: root.listLayout

                                    title: modelData.name
                                    iconName: modelData.icon
                                    tint: group.modelData.custom ? root.customTint(modelData.categoryId) : group.modelData.tint
                                    wordCount: modelData.wordCount
                                    learningCount: modelData.learningCount
                                    learnedCount: modelData.learnedCount

                                    onOpenRequested: group.modelData.custom ? root.userDeckOpened(modelData.id) : root.deckOpened(modelData.id)
                                    onHeld:
                                    {
                                        if (group.modelData.custom)
                                        {
                                            root.showDeckActions(modelData)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    MobileDecksSectionsSheet
    {
        id: sectionsSheet

        anchors.fill: parent
        z: 40

        section: root.section
        builtinCategories: root.builtinCategories
        builtinCount: DecksController.decks.length
        customCategories: root.customCategories
        myDecksCount: UserDecksController.decks.length

        onSectionChosen: function(section) { root.chooseSection(section) }
        onNewCategoryRequested: root.createCategory()
        onCategoryHeld: function(categoryId, name)
        {
            sectionsSheet.close()
            root.showCategoryActions(categoryId, name)
        }
        onImportRequested: importDialog.open()
    }

    MobileDecksSortSheet
    {
        id: sortSheet

        z: 50

        order: root.sortOrder
        reversed: root.sortReversed

        onSortChosen: function(order, reversed) { AppViewLayouts.setSort("userDecks", order, reversed) }
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

    MobileSettingsPicker
    {
        id: levelPicker

        anchors.fill: parent
        z: 60

        onHidden:
        {
            const action = root.afterPicker

            root.afterPicker = null

            if (action)
            {
                action()
            }
        }
    }

    MobileNameSheet
    {
        id: nameSheet

        z: 55
    }

    MobileConfirmSheet
    {
        id: confirmSheet
    }
}
