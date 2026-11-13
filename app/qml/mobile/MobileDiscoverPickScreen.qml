import QtQuick
import QtQuick.Layouts
import "../Decks.js" as Decks

Rectangle
{
    id: root

    required property var controller

    signal startRequested(var deckIds)
    signal done()

    readonly property int maximumPicked: 2

    property string category: ""
    property string selectedCategory: ""
    property string deckCategory: ""
    property var picked: []
    property real deckShift: 0

    readonly property var decks: DecksController.decks
    readonly property var categories: Decks.categories(root.decks)
    readonly property var categoryDecks: root.decks.filter(Deck => Deck.category === root.deckCategory)
    readonly property int pickedWordCount: root.categoryDecks
        .filter(Deck => root.picked.indexOf(Deck.id) >= 0)
        .reduce((Total, Deck) => Total + Deck.wordCount, 0)

    function categoryTint(category)
    {
        return Decks.tint(root.categories, category, AppTheme.coverColours)
    }

    function sum(category, field)
    {
        return root.decks.filter(Deck => Deck.category === category).reduce((Total, Deck) => Total + Deck[field], 0)
    }

    function firstIcon(category)
    {
        const deck = root.decks.find(Deck => Deck.category === category)

        return deck ? deck.icon : ""
    }

    function slideTo(target)
    {
        stepSlide.stop()

        if (AppMetrics.motionTransition <= 0)
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

    function back()
    {
        if (root.category.length > 0)
        {
            root.category = ""
            root.picked = []
            root.slideTo(0)

            return
        }

        root.done()
    }

    function toggle(deckId)
    {
        if (root.picked.indexOf(deckId) >= 0)
        {
            root.picked = root.picked.filter(Id => Id !== deckId)

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

    function restoreSaved()
    {
        DecksController.refresh()

        const saved = root.controller.savedDecks().map(Number)
        const first = root.decks.find(Deck => saved.indexOf(Deck.id) >= 0)

        stepSlide.stop()
        categoryPage.contentY = 0
        deckPage.contentY = 0

        if (!first)
        {
            root.category = ""
            root.selectedCategory = ""
            root.picked = []
            root.deckShift = 0

            return
        }

        root.category = first.category
        root.selectedCategory = first.category
        root.deckCategory = first.category
        root.deckShift = 1
        root.picked = root.decks
            .filter(Deck => Deck.category === first.category && Deck.untriagedCount > 0 && saved.indexOf(Deck.id) >= 0)
            .map(Deck => Deck.id)
            .slice(0, root.maximumPicked)
    }

    component PickPage: Flickable
    {
        id: pickPage

        property string heading: ""
        property alias tiles: pickGrid.data
        readonly property real tileWidth: pickGrid.tileWidth

        readonly property real gridTop: Math.max(pickHeading.implicitHeight + AppMetrics.gapLarge * 2,
                                                 (pickPage.height - pickGrid.implicitHeight) * AppMetrics.pickTopFill)

        contentWidth: width
        contentHeight: pickPage.gridTop + pickGrid.implicitHeight + AppMetrics.contentPadding
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        Text
        {
            id: pickHeading

            x: AppMetrics.contentPadding
            y: (pickPage.gridTop - pickHeading.implicitHeight) / 2
            width: pickPage.width - AppMetrics.contentPadding * 2
            text: pickPage.heading
            font.pixelSize: AppMetrics.fontPickTitle
            font.weight: Font.DemiBold
            font.family: AppMetrics.fontFamily
            color: AppTheme.colourText
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap

            Accessible.role: Accessible.Heading
            Accessible.name: pickHeading.text
        }

        DesktopCenteredFlow
        {
            id: pickGrid

            readonly property int columnCount: Math.max(2, Math.floor((width + spacing) / (AppMetrics.coverTileMinimumWidth + spacing)))
            readonly property real tileWidth: Math.max(0, Math.floor((width - spacing * (columnCount - 1)) / columnCount))

            x: AppMetrics.contentPadding
            y: pickPage.gridTop
            width: pickPage.width - AppMetrics.contentPadding * 2
            spacing: AppMetrics.gapCompact
        }
    }

    NumberAnimation
    {
        id: stepSlide

        target: root
        property: "deckShift"
        duration: AppMetrics.motionTransition
        easing.type: AppMetrics.motionEasing
    }

    color: AppTheme.colourPanel

    ColumnLayout
    {
        anchors.fill: parent
        spacing: 0

        MobileHeader
        {
            title: qsTr("Discover")
            showBack: true

            onBackRequested: root.back()
        }

        Item
        {
            id: pages

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: stepSlide.running

            ColumnLayout
            {
                x: -root.deckShift * pages.width
                width: pages.width
                height: pages.height
                visible: root.deckShift < 1
                spacing: 0

                PickPage
                {
                    id: categoryPage

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    heading: qsTr("Pick a category")

                    Accessible.role: Accessible.List
                    Accessible.name: qsTr("Categories")

                    tiles: Repeater
                    {
                        model: root.categories

                        delegate: MobileDeck
                        {
                            required property string modelData
                            required property int index

                            objectName: "discover_category_" + index

                            width: categoryPage.tileWidth
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

                MobileWideButton
                {
                    objectName: "discover_continue"

                    Layout.leftMargin: AppMetrics.contentPadding
                    Layout.rightMargin: AppMetrics.contentPadding
                    Layout.topMargin: AppMetrics.gapMedium
                    Layout.bottomMargin: AppMetrics.gapLarge
                    interactive: root.selectedCategory.length > 0
                    label: qsTr("Continue")

                    onClicked: root.openCategory(root.selectedCategory)
                }
            }

            ColumnLayout
            {
                x: (1 - root.deckShift) * pages.width
                width: pages.width
                height: pages.height
                visible: root.deckShift > 0
                spacing: 0

                PickPage
                {
                    id: deckPage

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    heading: qsTr("Pick up to two decks")

                    Accessible.role: Accessible.List
                    Accessible.name: root.deckCategory

                    tiles: Repeater
                    {
                        model: root.categoryDecks

                        delegate: MobileDeck
                        {
                            id: deckTile

                            required property var modelData

                            objectName: "discover_deck_" + modelData.id

                            width: deckPage.tileWidth
                            pickable: true
                            picked: root.picked.indexOf(modelData.id) >= 0
                            enabled: modelData.untriagedCount > 0
                            title: modelData.name
                            iconName: modelData.icon
                            tint: root.categoryTint(modelData.category)
                            wordCount: modelData.wordCount
                            learningCount: modelData.learningCount
                            learnedCount: modelData.learnedCount

                            onPickRequested: if (!root.toggle(modelData.id)) deckTile.wiggle()
                        }
                    }
                }

                MobileWideButton
                {
                    objectName: "discover_start"

                    Layout.leftMargin: AppMetrics.contentPadding
                    Layout.rightMargin: AppMetrics.contentPadding
                    Layout.topMargin: AppMetrics.gapMedium
                    Layout.bottomMargin: AppMetrics.gapLarge
                    interactive: root.picked.length > 0
                    label: root.picked.length > 0 ? qsTr("Start · %n words", "", root.pickedWordCount) : qsTr("Start")

                    onClicked: root.startRequested(root.picked)
                }
            }
        }
    }
}
