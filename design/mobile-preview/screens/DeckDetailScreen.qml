import QtQuick
import QtQuick.Layouts
import "../"

AppShell {
    id: root

    activeTab: "Decks"

    signal learnRequested()
    signal trainRequested()

    readonly property int wordsToLearn: 64

    headerTitle: "Mercado y comida"
    showBack: true
    trailingIcon: "⋯"
    showSearch: true

    readonly property var words: [
        { word: "le marché", translation: "market", stage: 3, completed: false },
        { word: "la cuillère", translation: "spoon", stage: 1, completed: false },
        { word: "le pain", translation: "bread", stage: 0, completed: false },
        { word: "la fraise", translation: "strawberry", stage: 2, completed: false },
        { word: "vendre", translation: "to sell", stage: 0, completed: false },
        { word: "le fromage", translation: "cheese", stage: 5, completed: true },
		{ word: "le marché", translation: "market", stage: 3, completed: false },
        { word: "la cuillère", translation: "spoon", stage: 1, completed: false },
        { word: "le pain", translation: "bread", stage: 0, completed: false },
        { word: "la fraise", translation: "strawberry", stage: 2, completed: false },
        { word: "vendre", translation: "to sell", stage: 0, completed: false },
        { word: "le fromage", translation: "cheese", stage: 5, completed: true },
		{ word: "le marché", translation: "market", stage: 3, completed: false },
        { word: "la cuillère", translation: "spoon", stage: 1, completed: false },
        { word: "le pain", translation: "bread", stage: 0, completed: false },
        { word: "la fraise", translation: "strawberry", stage: 2, completed: false },
        { word: "vendre", translation: "to sell", stage: 0, completed: false },
        { word: "le fromage", translation: "cheese", stage: 5, completed: true }
    ]

    readonly property var filterOptions: [
        { key: "notlearned", label: "Not learned" },
        { key: "learning", label: "Learning" },
        { key: "learned", label: "Learned" }
    ]

    property var selectedFilterKeys: []
    property bool filterSheetOpen: false

    function categoryOf(item)
    {
        if (item.completed)
        {
            return "learned"
        }

        if (item.stage <= 0)
        {
            return "notlearned"
        }

        return "learning"
    }

    function toggleFilterKey(key)
    {
        const index = root.selectedFilterKeys.indexOf(key)
        const next = root.selectedFilterKeys.slice()

        if (index >= 0)
        {
            next.splice(index, 1)
        }
        else
        {
            next.push(key)
        }

        root.selectedFilterKeys = next
    }

    readonly property string filterLabel:
    {
        if (root.selectedFilterKeys.length === 0)
        {
            return "All"
        }

        if (root.selectedFilterKeys.length === 1)
        {
            for (const option of root.filterOptions)
            {
                if (option.key === root.selectedFilterKeys[0])
                {
                    return option.label
                }
            }
        }

        return root.selectedFilterKeys.length + " selected"
    }

    readonly property var filteredWords:
    {
        if (root.selectedFilterKeys.length === 0)
        {
            return root.words
        }

        return root.words.filter(function(item)
        {
            return root.selectedFilterKeys.indexOf(root.categoryOf(item)) >= 0
        })
    }

    readonly property var sortOptions: [
        { key: "word", primary: "A-Z", inverted: "Z-A" },
        { key: "recent", primary: "Most Recent", inverted: "Oldest first" },
        { key: "added", primary: "Recently added", inverted: "First added" }
    ]

    property string sortKey: "word"
    property bool sortDescending: false

    property bool sortSheetOpen: false

    readonly property string sortLabel:
    {
        for (const option of root.sortOptions)
        {
            if (option.key === root.sortKey)
            {
                return root.sortDescending ? option.inverted : option.primary
            }
        }

        return ""
    }

    readonly property var sortedWords:
    {
        const list = root.filteredWords.slice()

        if (root.sortKey === "word")
        {
            list.sort(function(a, b) { return a.word.localeCompare(b.word) })
        }
        else if (root.sortKey === "recent")
        {
            list.reverse()
        }

        if (root.sortDescending)
        {
            list.reverse()
        }

        return list
    }

    Flickable {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: actionRow.top
        anchors.bottomMargin: 8
        contentWidth: width
        contentHeight: content.implicitHeight + 32
        clip: true

        ColumnLayout {
            id: content
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 16
            spacing: 16

            SearchFilterBar {
                searchPlaceholder: "Search words..."
                filterLabel: root.filterLabel
                sortLabel: root.sortLabel
                counterText: root.filteredWords.length + " words"

                onFilterRequested: root.filterSheetOpen = true
                onSortRequested: root.sortSheetOpen = true
            }

            WordCardList {
                model: root.sortedWords
            }
        }
    }

    WideButtonRow {
        id: actionRow

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 16

        WideButton {
            label: "Train"
            filled: false

            onClicked: root.trainRequested()
        }

        WideButton {
            label: "Learn · " + root.wordsToLearn

            onClicked: root.learnRequested()
        }
    }

    FloatingActionButton {
        anchors.right: parent.right
        anchors.bottom: actionRow.top
        anchors.margins: 16
        anchors.bottomMargin: 16 + 16
    }

    Loader {
        anchors.fill: parent
        z: 10
        active: root.filterSheetOpen

        sourceComponent: FilterSheet {
            model: root.filterOptions
            selectedKeys: root.selectedFilterKeys

            onDismissed: root.filterSheetOpen = false
            onToggled: function(key) { root.toggleFilterKey(key) }
            onCleared: root.selectedFilterKeys = []
        }
    }

    Loader {
        anchors.fill: parent
        z: 10
        active: root.sortSheetOpen

        sourceComponent: SortSheet {
            model: root.sortOptions
            currentKey: root.sortKey
            descending: root.sortDescending

            onDismissed: root.sortSheetOpen = false

            onChosen: function(key, desc)
            {
                root.sortKey = key
                root.sortDescending = desc
                root.sortSheetOpen = false
            }
        }
    }
}
