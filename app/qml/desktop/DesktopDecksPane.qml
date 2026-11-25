import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../Decks.js" as Decks

DesktopSidePane
{
    id: root

    property string section: "all"
    property var builtinCategories: []
    property int builtinCount: 0
    property var customCategories: []
    property int myDecksCount: 0

    signal sectionChosen(string section)
    signal newCategoryRequested()
    signal renameCategoryRequested(string categoryId, string name)
    signal deleteCategoryRequested(string categoryId, string name)
    signal importRequested()

    widthKey: "decksRail"
    defaultWidth: AppMetrics.categoryPaneWidth
    spacing: AppMetrics.gapCompact

    Accessible.role: Accessible.PageTabList
    Accessible.name: qsTr("Deck sections")

    component SectionItem: DesktopNavItem
    {
        required property string key

        objectName: "deck_section_" + key

        destination: false
        barAtBottom: false
        selected: root.section === key

        onClicked: root.sectionChosen(key)
    }

    RowLayout
    {
        Layout.fillWidth: true
        Layout.preferredHeight: AppMetrics.buttonHeight
        visible: Decks.customDecksShown()
        Layout.leftMargin: AppMetrics.gapCompact
        spacing: AppMetrics.gapSmall

        DesktopSectionLabel
        {
            Layout.fillWidth: true
            text: qsTr("My categories")
        }

        DesktopIconButton
        {
            objectName: "deck_new_category"

            iconName: "plus"
            iconSize: AppMetrics.iconSizeSmall
            iconColour: AppTheme.colourSubtle
            text: qsTr("New category")

            onClicked: root.newCategoryRequested()
        }
    }

    Item
    {
        id: railArea

        Layout.fillWidth: true
        Layout.fillHeight: true

        Flickable
        {
            id: scroller

            anchors.fill: parent
            contentWidth: width
            contentHeight: railColumn.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: DesktopScrollBar
            {
                parent: railArea
                x: railArea.width
                y: 0
                height: railArea.height
            }

            ColumnLayout
            {
                id: railColumn

                width: scroller.width
                spacing: AppMetrics.borderWidth

                SectionItem
                {
                    key: "mine"
                    visible: Decks.customDecksShown()
                    text: qsTr("My Decks")
                    dotColour: AppTheme.colourMuted
                    count: root.myDecksCount
                }

                Repeater
                {
                    model: Decks.customDecksShown() ? root.customCategories : []

                    delegate: SectionItem
                    {
                        id: customItem

                        required property var modelData

                        key: "mine:" + modelData.id
                        text: modelData.name
                        dotColour: modelData.tint
                        count: modelData.count

                        onPressAndHold: customMenu.popup(customItem, customItem.pressX, customItem.pressY)

                        TapHandler
                        {
                            acceptedButtons: Qt.RightButton
                            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad

                            onTapped: function(point) { customMenu.popup(customItem, point.position.x, point.position.y) }
                        }

                        DesktopMenu
                        {
                            id: customMenu

                            DesktopMenuItem
                            {
                                text: qsTr("Rename…")
                                iconName: "edit"

                                onTriggered: root.renameCategoryRequested(customItem.modelData.id, customItem.modelData.name)
                            }

                            DesktopMenuItem
                            {
                                text: qsTr("Delete category")
                                iconName: "trash"
                                destructive: true

                                onTriggered: root.deleteCategoryRequested(customItem.modelData.id, customItem.modelData.name)
                            }
                        }
                    }
                }

                RowLayout
                {
                    objectName: "deck_builtin_header"

                    Layout.fillWidth: true
                    Layout.topMargin: AppMetrics.gapCompact
                    Layout.preferredHeight: AppMetrics.navItemHeight
                    Layout.leftMargin: AppMetrics.gapCompact
                    Layout.rightMargin: AppMetrics.gapSmall
                    spacing: AppMetrics.gapSmall

                    DesktopSectionLabel
                    {
                        text: qsTr("Built-in decks")
                    }

                    DesktopDivider
                    {
                        Layout.fillWidth: true
                    }
                }

                ColumnLayout
                {
                    Layout.fillWidth: true
                    spacing: AppMetrics.borderWidth

                    SectionItem
                    {
                        key: "all"
                        text: qsTr("All")
                        dotColour: AppTheme.colourMuted
                        count: root.builtinCount
                    }

                    Repeater
                    {
                        model: root.builtinCategories

                        delegate: SectionItem
                        {
                            required property var modelData

                            key: "category:" + modelData.name
                            text: modelData.name
                            dotColour: modelData.tint
                            count: modelData.count
                        }
                    }
                }
            }
        }
    }

    DesktopButton
    {
        objectName: "deck_import"

        Layout.fillWidth: true
        visible: Decks.customDecksShown()
        tone: "primary"
        iconName: "plus"
        text: qsTr("Import")

        onClicked: root.importRequested()
    }
}
