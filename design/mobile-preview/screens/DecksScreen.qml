import QtQuick
import QtQuick.Layouts
import "../"

AppShell {
    id: root

    activeTab: "Decks"
    showSearch: true
    showTabBar: true

    readonly property var decks: [
        { name: "Mercado y comida", colourIndex: 0, total: 120, notStarted: 40, learning: 32, completed: 48 },
        { name: "Transporte", colourIndex: 1, total: 96, notStarted: 40, learning: 30, completed: 26 },
        { name: "Verbos irregulares", colourIndex: 2, total: 64, notStarted: 4, learning: 8, completed: 52 },
        { name: "Casa y muebles", colourIndex: 3, total: 112, notStarted: 0, learning: 0, completed: 112 },
        { name: "Salud y cuerpo", colourIndex: 0, total: 84, notStarted: 84, learning: 0, completed: 0 }
    ]

    readonly property var filterOptions: [
        { key: "notstarted", label: "Not started" },
        { key: "learning", label: "Learning" },
        { key: "mastered", label: "Mastered" }
    ]

    property var selectedFilterKeys: []
    property bool filterSheetOpen: false

    function categoryOf(deck)
    {
        if (deck.completed >= deck.total)
        {
            return "mastered"
        }

        if (deck.notStarted >= deck.total)
        {
            return "notstarted"
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

    readonly property var filteredDecks:
    {
        if (root.selectedFilterKeys.length === 0)
        {
            return root.decks
        }

        return root.decks.filter(function(deck)
        {
            return root.selectedFilterKeys.indexOf(root.categoryOf(deck)) >= 0
        })
    }

    readonly property var sortOptions: [
        { key: "due", primary: "Most due", inverted: "Least due" },
        { key: "name", primary: "A-Z", inverted: "Z-A" }
    ]

    property string sortKey: "due"
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

    readonly property var sortedDecks:
    {
        const list = root.filteredDecks.slice()

        if (root.sortKey === "due")
        {
            list.sort(function(a, b)
            {
                return (b.notStarted + b.learning) - (a.notStarted + a.learning)
            })
        }
        else if (root.sortKey === "name")
        {
            list.sort(function(a, b) { return a.name.localeCompare(b.name) })
        }

        if (root.sortDescending)
        {
            list.reverse()
        }

        return list
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        SearchFilterBar {
            searchPlaceholder: "Search decks..."
            filterLabel: root.filterLabel
            sortLabel: root.sortLabel
            counterText: root.filteredDecks.length + " decks"

            onFilterRequested: root.filterSheetOpen = true
            onSortRequested: root.sortSheetOpen = true
        }

        GridView {
            id: deckGrid
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            readonly property real cellSpacing: 12

            cellWidth: (width - cellSpacing) / 2
            cellHeight: 143 + cellSpacing

            model: root.sortedDecks

            delegate: Item {
                required property var modelData

                width: deckGrid.cellWidth
                height: deckGrid.cellHeight

                Deck {
                    width: parent.width - deckGrid.cellSpacing

                    name: modelData.name
                    colourIndex: modelData.colourIndex
                    total: modelData.total
                    notStarted: modelData.notStarted
                    learning: modelData.learning
                    completed: modelData.completed
                }
            }
        }
    }

    FloatingActionButton {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 16
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
