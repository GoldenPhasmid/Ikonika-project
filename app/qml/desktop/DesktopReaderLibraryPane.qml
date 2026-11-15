import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

DesktopSidePane
{
    id: root

    property string section: "all"
    property var counts: ({})
    property var collections: []
    property bool freeShown: false

    signal sectionChosen(string section)
    signal newCollectionRequested()
    signal renameCollectionRequested(string collectionId, string name)
    signal deleteCollectionRequested(string collectionId, string name)
    signal importRequested()

    widthKey: "readerLibrary"
    defaultWidth: AppMetrics.categoryPaneWidth
    spacing: AppMetrics.borderWidth

    Accessible.role: Accessible.PageTabList
    Accessible.name: qsTr("Library sections")

    component SectionItem: DesktopNavItem
    {
        required property string key

        objectName: "reader_section_" + key
        destination: false
        barAtBottom: false
        selected: root.section === key
        count: root.counts[key] || 0

        onClicked: root.sectionChosen(key)
    }

    DesktopSectionLabel
    {
        Layout.leftMargin: AppMetrics.gapCompact
        Layout.bottomMargin: AppMetrics.gapTight
        text: qsTr("Library")
    }

    SectionItem
    {
        key: "all"
        text: qsTr("My books")
        iconName: "library"
    }

    SectionItem
    {
        visible: root.freeShown
        key: "free"
        text: qsTr("Free library")
        iconName: "globe"
    }

    SectionItem
    {
        key: "reading"
        text: qsTr("Reading")
        iconName: "nav_reader"
    }

    SectionItem
    {
        key: "finished"
        text: qsTr("Finished")
        iconName: "circle_check"
    }

    SectionItem
    {
        key: "favorites"
        text: qsTr("Favorites")
        iconName: "heart"
    }

    RowLayout
    {
        Layout.fillWidth: true
        Layout.leftMargin: AppMetrics.gapCompact
        Layout.topMargin: AppMetrics.gapMedium
        spacing: AppMetrics.gapSmall

        DesktopSectionLabel
        {
            Layout.fillWidth: true
            text: qsTr("Collections")
        }

        DesktopIconButton
        {
            objectName: "reader_new_collection"

            iconName: "plus"
            iconSize: AppMetrics.iconSizeSmall
            iconColour: AppTheme.colourSubtle
            text: qsTr("New collection")

            onClicked: root.newCollectionRequested()
        }
    }

    ListView
    {
        id: collectionList

        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        spacing: AppMetrics.borderWidth
        boundsBehavior: Flickable.StopAtBounds
        model: root.collections

        ScrollBar.vertical: DesktopScrollBar {}

        delegate: DesktopNavItem
        {
            id: collectionItem

            required property var modelData
            required property int index

            objectName: "reader_collection_" + index

            width: collectionList.width
            destination: false
            barAtBottom: false
            text: modelData.name
            dotColour: AppTheme.coverColours[modelData.colour % AppTheme.coverColours.length]
            selected: root.section === "collection:" + modelData.id
            count: modelData.count

            onClicked: root.sectionChosen("collection:" + modelData.id)

            TapHandler
            {
                acceptedButtons: Qt.RightButton

                onTapped: function(point) { collectionMenu.popup(collectionItem, point.position.x, point.position.y) }
            }

            DesktopMenu
            {
                id: collectionMenu

                DesktopMenuItem
                {
                    text: qsTr("Rename…")
                    iconName: "edit"

                    onTriggered: root.renameCollectionRequested(collectionItem.modelData.id, collectionItem.modelData.name)
                }

                DesktopMenuItem
                {
                    text: qsTr("Delete collection")
                    iconName: "trash"
                    destructive: true

                    onTriggered: root.deleteCollectionRequested(collectionItem.modelData.id, collectionItem.modelData.name)
                }
            }
        }
    }

    DesktopButton
    {
        objectName: "reader_import"

        Layout.fillWidth: true
        tone: "primary"
        iconName: "plus"
        text: qsTr("Import a book")

        onClicked: root.importRequested()
    }
}
