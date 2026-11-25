import QtQuick
import QtQuick.Layouts

MobileSectionsDrawer
{
    id: root

    property string section: "all"
    property var counts: ({})
    property var collections: []
    property bool freeShown: false

    signal sectionChosen(string section)
    signal newCollectionRequested()
    signal collectionHeld(string collectionId, string name)
    signal importRequested()

    function choose(section)
    {
        root.sectionChosen(section)
        root.close()
    }

    accessibleName: qsTr("Library sections")

    component SectionRow: MobileSectionRow
    {
        objectName: "reader_section_" + key
        current: root.section
        count: root.counts[key] || 0

        onChosen: function(key) { root.choose(key) }
    }

    MobileSectionHeading
    {
        text: qsTr("Library")
    }

    SectionRow
    {
        key: "all"
        label: qsTr("My books")
        iconName: "library"
    }

    SectionRow
    {
        visible: root.freeShown
        key: "free"
        label: qsTr("Free library")
        iconName: "globe"
    }

    SectionRow
    {
        key: "reading"
        label: qsTr("Reading")
        iconName: "nav_reader"
    }

    SectionRow
    {
        key: "finished"
        label: qsTr("Finished")
        iconName: "circle_check"
    }

    SectionRow
    {
        key: "favorites"
        label: qsTr("Favorites")
        iconName: "heart"
    }

    Rectangle
    {
        Layout.fillWidth: true
        Layout.leftMargin: AppMetrics.contentPadding
        Layout.rightMargin: AppMetrics.contentPadding
        Layout.topMargin: AppMetrics.gapSmall
        Layout.bottomMargin: AppMetrics.gapSmall
        Layout.preferredHeight: AppMetrics.borderWidth
        color: AppTheme.colourBorder
    }

    RowLayout
    {
        Layout.fillWidth: true
        spacing: 0

        MobileSectionHeading
        {
            Layout.topMargin: 0
            Layout.bottomMargin: 0
            text: qsTr("Collections")
        }

        MobileIconButton
        {
            objectName: "reader_new_collection"

            iconName: "plus"
            iconSize: AppMetrics.fontLabel
            iconColour: AppTheme.colourSubtle
            text: qsTr("New collection")

            onClicked: root.newCollectionRequested()
        }
    }

    Flickable
    {
        Layout.fillWidth: true
        Layout.fillHeight: true
        contentWidth: width
        contentHeight: collectionColumn.implicitHeight
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        ColumnLayout
        {
            id: collectionColumn

            width: parent.width
            spacing: 0

            Repeater
            {
                model: root.collections

                delegate: SectionRow
                {
                    required property var modelData

                    key: "collection:" + modelData.id
                    label: modelData.name
                    dotColour: AppTheme.coverColours[modelData.colour % AppTheme.coverColours.length]
                    count: modelData.count

                    onHeld: root.collectionHeld(modelData.id, modelData.name)
                }
            }
        }
    }

    MobileWideButton
    {
        objectName: "reader_import"

        Layout.leftMargin: AppMetrics.gapSmall
        Layout.rightMargin: AppMetrics.gapSmall
        label: qsTr("Import a book")

        onClicked:
        {
            root.close()
            root.importRequested()
        }
    }
}
