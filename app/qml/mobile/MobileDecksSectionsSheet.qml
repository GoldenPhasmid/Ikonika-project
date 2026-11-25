import QtQuick
import QtQuick.Layouts
import "../Decks.js" as Decks

MobileSectionsDrawer
{
    id: root

    property string section: "all"
    property var builtinCategories: []
    property int builtinCount: 0
    property var customCategories: []
    property int myDecksCount: 0

    signal sectionChosen(string section)
    signal newCategoryRequested()
    signal categoryHeld(string categoryId, string name)
    signal importRequested()

    function choose(section)
    {
        root.sectionChosen(section)
        root.close()
    }

    accessibleName: qsTr("Deck sections")

    component SectionRow: MobileSectionRow
    {
        objectName: "deck_section_" + key
        current: root.section

        onChosen: function(key) { root.choose(key) }
    }

    Flickable
    {
        Layout.fillWidth: true
        Layout.fillHeight: true
        contentWidth: width
        contentHeight: sectionColumn.implicitHeight
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        ColumnLayout
        {
            id: sectionColumn

            width: parent.width
            spacing: 0

            RowLayout
            {
                Layout.fillWidth: true
                visible: Decks.customDecksShown()
                spacing: 0

                MobileSectionHeading
                {
                    Layout.topMargin: 0
                    Layout.bottomMargin: 0
                    text: qsTr("My categories")
                }

                MobileIconButton
                {
                    objectName: "deck_new_category"

                    iconName: "plus"
                    iconSize: AppMetrics.fontLabel
                    iconColour: AppTheme.colourSubtle
                    text: qsTr("New category")

                    onClicked: root.newCategoryRequested()
                }
            }

            SectionRow
            {
                key: "mine"
                visible: Decks.customDecksShown()
                label: qsTr("My Decks")
                dotColour: AppTheme.colourMuted
                count: root.myDecksCount
            }

            Repeater
            {
                model: Decks.customDecksShown() ? root.customCategories : []

                delegate: SectionRow
                {
                    required property var modelData

                    key: "mine:" + modelData.id
                    label: modelData.name
                    dotColour: modelData.tint
                    count: modelData.count

                    onHeld: root.categoryHeld(modelData.id, modelData.name)
                }
            }

            Item
            {
                objectName: "deck_builtin_header"

                Layout.fillWidth: true
                Layout.topMargin: AppMetrics.gapSmall
                Layout.preferredHeight: AppMetrics.buttonHeightLarge

                RowLayout
                {
                    anchors.fill: parent
                    anchors.leftMargin: AppMetrics.contentPadding
                    anchors.rightMargin: AppMetrics.contentPadding
                    spacing: AppMetrics.gapMedium

                    MobileSectionHeading
                    {
                        Layout.fillWidth: false
                        Layout.leftMargin: 0
                        Layout.topMargin: 0
                        Layout.bottomMargin: 0
                        text: qsTr("Built-in decks")
                    }

                    Rectangle
                    {
                        Layout.fillWidth: true
                        Layout.preferredHeight: AppMetrics.borderWidth
                        color: AppTheme.colourBorder
                    }
                }
            }

            ColumnLayout
            {
                Layout.fillWidth: true
                spacing: 0

                SectionRow
                {
                    key: "all"
                    label: qsTr("All")
                    dotColour: AppTheme.colourMuted
                    count: root.builtinCount
                }

                Repeater
                {
                    model: root.builtinCategories

                    delegate: SectionRow
                    {
                        required property var modelData

                        key: "category:" + modelData.name
                        label: modelData.name
                        dotColour: modelData.tint
                        count: modelData.count
                    }
                }
            }
        }
    }

    MobileWideButton
    {
        objectName: "deck_import"

        visible: Decks.customDecksShown()
        Layout.leftMargin: AppMetrics.gapSmall
        Layout.rightMargin: AppMetrics.gapSmall
        Layout.topMargin: AppMetrics.gapSmall
        label: qsTr("Import")

        onClicked:
        {
            root.close()
            root.importRequested()
        }
    }
}
