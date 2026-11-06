import QtQuick
import QtQuick.Layouts
import "../Decks.js" as Decks

Item
{
    id: root

    required property var store

    property var deckId: 0
    readonly property bool deckOpen: root.deckId > 0
    readonly property var deckVocabulary: deckLoader.item ? deckLoader.item.vocabulary : null
    readonly property string deckName: root.deckVocabulary ? root.deckVocabulary.deckName : ""
    readonly property int deckNewCount: root.deckVocabulary ? root.deckVocabulary.deckNewCount : 0

    signal dictionaryRequested(string word, string partOfSpeech)
    signal removeCardRequested(string cardId, string word)
    signal cardRemoved(string cardId)
    signal trainRequested(var deckId, Item source)

    function deckFace(deckId)
    {
        for (let sectionIndex = 0; sectionIndex < sectionRepeater.count; ++sectionIndex)
        {
            const section = sectionRepeater.itemAt(sectionIndex)

            for (let index = 0; section && index < section.tiles.count; ++index)
            {
                const tile = section.tiles.itemAt(index)

                if (tile && tile.modelData.id === deckId)
                {
                    return tile.face
                }
            }
        }

        return null
    }

    function refresh()
    {
        DecksController.refresh()

        if (deckLoader.item)
        {
            deckLoader.item.refresh()
        }
    }

    function openDeck(deckId)
    {
        root.deckId = deckId
    }

    function closeDeck()
    {
        root.deckId = 0
    }

    property string category: ""

    readonly property var categories: Decks.categories(DecksController.decks)

    readonly property var sections:
    {
        if (root.categories.length === 0)
        {
            return [""]
        }

        return root.category.length === 0 ? root.categories : [root.category]
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
        if (root.categories.indexOf(root.category) < 0)
        {
            root.category = ""
        }
    }

    ColumnLayout
    {
        anchors.fill: parent
        visible: !root.deckOpen
        spacing: 0

        DesktopChipRow
        {
            id: filterBar

            Layout.fillWidth: false
            Layout.preferredWidth: page.columnWidth + filterBar.ringMargin * 2
            Layout.alignment: Qt.AlignHCenter
            Layout.leftMargin: 0
            Layout.rightMargin: 0
            Layout.topMargin: AppMetrics.contentPadding - filterBar.ringMargin
            z: 1
            visible: root.categories.length > 1
            spacing: AppMetrics.gapTight

            Accessible.role: Accessible.ToolBar
            Accessible.name: qsTr("Filter decks")

            DesktopChip
            {
                objectName: "deck_category_all"

                width: Math.max(implicitWidth, filterBar.chipWidth)
                text: qsTr("All")
                count: DecksController.decks.length.toString()
                selected: root.category.length === 0

                onClicked: root.category = ""
            }

            Repeater
            {
                model: root.categories

                delegate: DesktopChip
                {
                    required property string modelData

                    width: Math.max(implicitWidth, filterBar.chipWidth)
                    text: modelData
                    dotColour: root.categoryTint(modelData)
                    count: root.countDecks(modelData).toString()
                    selected: root.category === modelData

                    onClicked: root.category = modelData
                }
            }
        }

        DesktopPage
        {
            id: page

            Layout.fillWidth: true
            Layout.fillHeight: true
            maximumWidth: AppMetrics.coverGridWidth
            topSpacing: filterBar.visible ? AppMetrics.gapMedium : AppMetrics.contentPadding
            spacing: AppMetrics.gapMedium

            Repeater
            {
                id: sectionRepeater

                model: root.sections

                delegate: ColumnLayout
                {
                    id: section

                    required property string modelData
                    required property int index

                    readonly property Item tiles: deckRepeater

                    Layout.fillWidth: true
                    Layout.topMargin: section.index > 0 ? AppMetrics.gapMedium : 0
                    spacing: AppMetrics.gapSmall

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

                            model: DecksController.decks.filter(deck => deck.category === section.modelData)

                            delegate: DesktopDeckTile
                            {
                                id: deckTile

                                required property var modelData

                                objectName: "deck_" + modelData.id

                                width: root.tileWidth
                                title: modelData.name
                                iconName: modelData.icon
                                tint: root.categoryTint(modelData.category)
                                wordCount: modelData.wordCount
                                learningCount: modelData.learningCount
                                learnedCount: modelData.learnedCount
                                untriagedCount: modelData.untriagedCount

                                onOpenRequested: root.openDeck(modelData.id)
                                onTrainRequested: root.trainRequested(modelData.id, deckTile.face)
                            }
                        }
                    }
                }
            }
        }
    }

    Loader
    {
        id: deckLoader

        anchors.fill: parent
        active: root.deckOpen

        onLoaded: deckLoader.item.resetView()

        sourceComponent: DesktopVocabularyScreen
        {
            store: root.store
            deckId: root.deckId

            onDictionaryRequested: function(word, partOfSpeech) { root.dictionaryRequested(word, partOfSpeech) }
            onRemoveCardRequested: function(cardId, word) { root.removeCardRequested(cardId, word) }
            onCardRemoved: function(cardId) { root.cardRemoved(cardId) }
        }
    }
}
