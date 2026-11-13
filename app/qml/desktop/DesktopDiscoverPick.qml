import QtQuick
import QtQuick.Layouts
import "../Decks.js" as Decks

FocusScope
{
    id: root

    clip: true

    required property var controller

    signal startRequested(var deckIds, Item face)
    signal leaveRequested()

    readonly property int maximumPicked: 2

    property string category: ""
    property string selectedCategory: ""
    property string deckCategory: ""
    property var picked: []
    property real deckShift: 0

    readonly property var decks: DecksController.decks
    readonly property var categories: Decks.categories(root.decks)
    readonly property var categoryDecks: root.decks.filter(deck => deck.category === root.deckCategory)
    readonly property int pickedWordCount: root.categoryDecks
        .filter(deck => root.picked.indexOf(deck.id) >= 0)
        .reduce((total, deck) => total + deck.wordCount, 0)

    readonly property int tileColumns: Math.max(1, Math.floor((categoryPage.columnWidth + AppMetrics.gapMedium)
                                                              / (AppMetrics.coverTileMinimumWidth + AppMetrics.gapMedium)))
    readonly property real tileWidth: Math.floor((categoryPage.columnWidth - AppMetrics.gapMedium * (root.tileColumns - 1))
                                                 / root.tileColumns)
    readonly property real gridTop: (categoryPage.height - categoryFlow.implicitHeight) * AppMetrics.pickTopFill
    readonly property real headerGap: Math.max(AppMetrics.gapLarge, (root.gridTop - categoryHeader.implicitHeight) / 2)

    function categoryTint(category)
    {
        return Decks.tint(root.categories, category, AppTheme.coverColours)
    }

    function sum(category, field)
    {
        return root.decks.filter(deck => deck.category === category).reduce((total, deck) => total + deck[field], 0)
    }

    function firstIcon(category)
    {
        const deck = root.decks.find(deck => deck.category === category)

        return deck ? deck.icon : ""
    }

    function slideTo(target)
    {
        stepSlide.stop()

        if (AppMetrics.motionPage <= 0)
        {
            root.deckShift = target

            return
        }

        stepSlide.to = target
        stepSlide.start()
    }

    function selectCategory(category)
    {
        console.info("Discover: selected category", category)
        root.selectedCategory = category
    }

    function openCategory(category)
    {
        console.info("Discover: opened category", category)
        root.category = category
        root.deckCategory = category
        root.picked = []
        deckPage.contentY = 0
        root.slideTo(1)
    }

    function showCategories()
    {
        root.category = ""
        root.picked = []
        root.slideTo(0)
    }

    function toggle(deckId)
    {
        if (root.picked.indexOf(deckId) >= 0)
        {
            root.picked = root.picked.filter(id => id !== deckId)

            return true
        }

        if (root.picked.length >= root.maximumPicked)
        {
            console.info("Discover: refused a deck past the limit of", root.maximumPicked)

            return false
        }

        root.picked = root.picked.concat([deckId])

        return true
    }

    function deckFace(deckId)
    {
        for (let index = 0; index < deckTiles.count; ++index)
        {
            const tile = deckTiles.itemAt(index)

            if (tile && tile.modelData.id === deckId)
            {
                return tile.face
            }
        }

        return null
    }

    function start()
    {
        if (root.picked.length > 0)
        {
            root.startRequested(root.picked, root.deckFace(root.picked[0]))
        }
    }

    function restoreSaved()
    {
        const saved = root.controller.savedDecks().map(Number)
        const first = root.decks.find(deck => saved.indexOf(deck.id) >= 0)

        if (!first)
        {
            return
        }

        root.category = first.category
        root.selectedCategory = first.category
        root.deckCategory = first.category
        root.deckShift = 1
        root.picked = root.decks
            .filter(deck => deck.category === first.category && deck.untriagedCount > 0 && saved.indexOf(deck.id) >= 0)
            .map(deck => deck.id)
            .slice(0, root.maximumPicked)
    }

    Component.onCompleted:
    {
        DecksController.refresh()
        root.restoreSaved()
    }

    Keys.onPressed: function(event)
    {
        if (event.key === Qt.Key_Escape)
        {
            root.leaveRequested()
            event.accepted = true

            return
        }

        if (event.key === Qt.Key_Backspace && root.category.length > 0)
        {
            root.showCategories()
            event.accepted = true

            return
        }

        if (event.key !== Qt.Key_Return && event.key !== Qt.Key_Enter)
        {
            return
        }

        if (root.category.length > 0)
        {
            root.start()
            event.accepted = true

            return
        }

        if (root.selectedCategory.length > 0)
        {
            root.openCategory(root.selectedCategory)
            event.accepted = true
        }
    }

    NumberAnimation
    {
        id: stepSlide

        target: root
        property: "deckShift"
        duration: AppMetrics.motionPage
        easing.type: AppMetrics.motionEasing
    }

    ColumnLayout
    {
        width: root.width
        height: root.height
        x: -root.deckShift * root.width
        spacing: 0

        DesktopPage
        {
            id: categoryPage

            Layout.fillWidth: true
            Layout.fillHeight: true
            maximumWidth: AppMetrics.coverGridWidth
            topSpacing: root.headerGap
            spacing: root.headerGap

            DesktopLabel
            {
                id: categoryHeader

                Layout.fillWidth: true
                text: qsTr("Pick a category")
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: AppMetrics.fontPickTitle
                font.weight: Font.Medium
            }

            DesktopCenteredFlow
            {
                id: categoryFlow

                Layout.fillWidth: true
                spacing: AppMetrics.gapMedium

                Accessible.role: Accessible.List
                Accessible.name: qsTr("Categories")

                Repeater
                {
                    model: root.categories

                    delegate: DesktopDeckTile
                    {
                        required property string modelData
                        required property int index

                        objectName: "discover_category_" + index

                        width: root.tileWidth
                        pickable: true
                        picked: root.selectedCategory === modelData
                        title: modelData
                        iconName: root.firstIcon(modelData)
                        tint: root.categoryTint(modelData)
                        wordCount: root.sum(modelData, "wordCount")
                        learningCount: root.sum(modelData, "learningCount")
                        learnedCount: root.sum(modelData, "learnedCount")

                        onPickRequested: root.selectCategory(modelData)
                    }
                }
            }
        }

        Item
        {
            Layout.fillWidth: true
            implicitHeight: pickedCount.implicitHeight + pickedCount.Layout.bottomMargin
        }

        DesktopActionBar
        {
            Layout.fillWidth: true

            DesktopButton
            {
                objectName: "discover_continue"

                Layout.preferredWidth: AppMetrics.actionButtonWidth
                large: true
                tone: "primary"
                enabled: root.selectedCategory.length > 0
                text: qsTr("Continue")
                keyHint: qsTr("Enter")

                onClicked: root.openCategory(root.selectedCategory)
            }
        }
    }

    ColumnLayout
    {
        width: root.width
        height: root.height
        x: (1 - root.deckShift) * root.width
        spacing: 0

        DesktopPage
        {
            id: deckPage

            Layout.fillWidth: true
            Layout.fillHeight: true
            maximumWidth: AppMetrics.coverGridWidth
            topSpacing: root.headerGap
            spacing: root.headerGap

            DesktopLabel
            {
                Layout.fillWidth: true
                text: qsTr("Pick up to two decks")
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: AppMetrics.fontPickTitle
                font.weight: Font.Medium
            }

            DesktopCenteredFlow
            {
                Layout.fillWidth: true
                spacing: AppMetrics.gapMedium

                Accessible.role: Accessible.List
                Accessible.name: root.deckCategory

                Repeater
                {
                    id: deckTiles

                    model: root.categoryDecks

                    delegate: DesktopDeckTile
                    {
                        id: deckTile

                        required property var modelData

                        objectName: "discover_deck_" + modelData.id

                        width: root.tileWidth
                        pickable: true
                        picked: root.picked.indexOf(modelData.id) >= 0
                        enabled: modelData.untriagedCount > 0
                        title: modelData.name
                        iconName: modelData.icon
                        tint: root.categoryTint(modelData.category)
                        wordCount: modelData.wordCount
                        learningCount: modelData.learningCount
                        learnedCount: modelData.learnedCount

                        onPickRequested:
                        {
                            if (!root.toggle(modelData.id))
                            {
                                refusal.restart()
                            }
                        }

                        DesktopWiggleAnimation
                        {
                            id: refusal

                            target: deckTile
                        }
                    }
                }
            }
        }

        DesktopLabel
        {
            id: pickedCount

            Layout.fillWidth: true
            Layout.bottomMargin: AppMetrics.gapMedium
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: AppMetrics.fontSubtitle
            color: AppTheme.colourSubtle
            opacity: root.picked.length > 0 ? 1 : 0
            text: qsTr("%n words", "", root.pickedWordCount)

            Behavior on opacity { NumberAnimation { duration: AppMetrics.motionFast } }
        }

        DesktopActionBar
        {
            id: actionBar

            Layout.fillWidth: true

            DesktopButton
            {
                objectName: "discover_start"

                Layout.preferredWidth: AppMetrics.actionButtonWidth
                large: true
                tone: "primary"
                enabled: root.picked.length > 0
                text: qsTr("Start")
                keyHint: qsTr("Enter")

                onClicked: root.start()
            }
        }
    }
}
