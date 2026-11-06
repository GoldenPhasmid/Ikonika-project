import QtQuick
import QtQuick.Layouts
import "MobileIcons.js" as Icons
import "../Decks.js" as Decks

Item
{
    id: root

    signal deckOpened(var deckId)
    signal trainRequested(var deckId)
    signal tabSelected(string key)
    signal searchRequested()

    property string category: ""

    readonly property var categories: Decks.categories(DecksController.decks)
    readonly property bool listLayout: AppViewLayouts.revision >= 0 && AppViewLayouts.isList("decks")
    readonly property real tileWidth: root.listLayout ? sectionColumn.width
                                                      : Math.floor((sectionColumn.width - AppMetrics.gapCompact) / 2)

    readonly property var shownCategories: root.category.length > 0 ? [root.category] : root.categories

    function decksIn(category)
    {
        return DecksController.decks.filter(deck => deck.category === category)
    }

    function refresh()
    {
        DecksController.refresh()
    }

    onCategoriesChanged:
    {
        if (root.categories.indexOf(root.category) < 0)
        {
            root.category = ""
        }
    }

    MobileShell
    {
        anchors.fill: parent

        activeTab: "decks"
        headerTitle: qsTr("Decks")
        showSearch: true

        onTabSelected: function(key) { root.tabSelected(key) }
        onSearchRequested: root.searchRequested()

        RowLayout
        {
            id: chipBar

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: AppMetrics.gapCompact
            anchors.rightMargin: AppMetrics.contentPadding - AppMetrics.gapSmall
            height: AppMetrics.chipHeight
            spacing: 0

            ListView
            {
                id: chipRow

                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: root.categories.length > 1

                orientation: ListView.Horizontal
                spacing: AppMetrics.gapSmall
                leftMargin: AppMetrics.contentPadding
                rightMargin: AppMetrics.gapSmall
                boundsBehavior: Flickable.StopAtBounds
                clip: true

                Accessible.role: Accessible.ToolBar
                Accessible.name: qsTr("Deck categories")

                model: [""].concat(root.categories)

                delegate: MobileFilterChip
                {
                    required property string modelData

                    objectName: modelData.length > 0 ? "deck_category_" + modelData : "deck_category_all"

                    label: modelData.length > 0 ? modelData : qsTr("All")
                    count: (modelData.length > 0 ? root.decksIn(modelData).length : DecksController.decks.length).toString()
                    dotColour: modelData.length > 0 ? Decks.tint(root.categories, modelData, AppTheme.coverColours) : "transparent"
                    selected: root.category === modelData

                    onClicked:
                    {
                        root.category = modelData
                        deckPage.contentY = 0
                    }
                }
            }

            Item
            {
                objectName: "decks_layout_switch"

                Layout.preferredWidth: AppMetrics.chipHeight
                Layout.preferredHeight: AppMetrics.chipHeight

                Accessible.role: Accessible.Button
                Accessible.name: root.listLayout ? qsTr("Show as tiles") : qsTr("Show as a list")

                MobileVectorIcon
                {
                    anchors.centerIn: parent
                    width: AppMetrics.headerActionIconSize + 2
                    height: width

                    svgPath: root.listLayout ? Icons.Icons.layout_grid.path : Icons.Icons.contents.path
                    color: AppTheme.colourText
                }

                TapHandler
                {
                    margin: AppMetrics.gapSmall

                    onTapped: AppViewLayouts.setList("decks", !root.listLayout)
                }
            }
        }

        Flickable
        {
            id: deckPage

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: chipBar.bottom
            anchors.bottom: parent.bottom
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

                Repeater
                {
                    model: root.shownCategories

                    delegate: ColumnLayout
                    {
                        id: section

                        required property string modelData

                        readonly property var decks: root.decksIn(section.modelData)

                        Layout.fillWidth: true
                        spacing: root.listLayout ? AppMetrics.gapTight : AppMetrics.gapCompact

                        RowLayout
                        {
                            Layout.fillWidth: true
                            visible: section.modelData.length > 0
                            spacing: AppMetrics.gapSmall

                            CategoryDot
                            {
                                colour: Decks.tint(root.categories, section.modelData, AppTheme.coverColours)
                            }

                            MobileSectionLabel
                            {
                                text: section.modelData.toUpperCase()
                            }

                            Rectangle
                            {
                                Layout.fillWidth: true
                                Layout.preferredHeight: AppMetrics.borderWidth
                                color: AppTheme.colourBorder
                            }

                            Text
                            {
                                text: Decks.summary(DecksController.decks, section.modelData, Qt.locale())
                                font.pixelSize: AppMetrics.fontMicro
                                font.family: AppMetrics.fontFamily
                                color: AppTheme.colourMuted
                            }
                        }

                        Flow
                        {
                            Layout.fillWidth: true
                            spacing: root.listLayout ? 0 : AppMetrics.gapCompact

                            Accessible.role: Accessible.List
                            Accessible.name: section.modelData.length > 0 ? section.modelData : qsTr("Decks")

                            Repeater
                            {
                                model: section.decks

                                delegate: MobileDeck
                                {
                                    required property var modelData

                                    objectName: "deck_" + modelData.id

                                    width: root.tileWidth
                                    compact: root.listLayout

                                    title: modelData.name
                                    iconName: modelData.icon
                                    tint: Decks.tint(root.categories, section.modelData, AppTheme.coverColours)
                                    wordCount: modelData.wordCount
                                    learningCount: modelData.learningCount
                                    learnedCount: modelData.learnedCount
                                    untriagedCount: modelData.untriagedCount

                                    onTrainRequested: root.trainRequested(modelData.id)
                                    onOpenRequested: root.deckOpened(modelData.id)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
