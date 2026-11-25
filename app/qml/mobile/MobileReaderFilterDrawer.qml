import QtQuick
import QtQuick.Layouts
import "../ReaderShelf.js" as Shelf

MobileReaderOptionsDrawer
{
    id: root

    property var filter: ({ "levels": [], "statuses": [], "storage": [], "formats": [] })
    property var formats: []
    property int resultCount: 0

    signal toggled(string key, string value)
    signal reset()

    heading: qsTr("Filter")

    component FilterGroup: ColumnLayout
    {
        id: group

        required property string key
        required property string title
        required property var values
        required property var labels

        Layout.fillWidth: true
        visible: group.values.length > 0
        spacing: AppMetrics.gapSmall

        Text
        {
            text: group.title
            font.pixelSize: AppMetrics.fontMicro
            font.weight: Font.DemiBold
            font.family: AppMetrics.fontFamily
            font.letterSpacing: AppMetrics.trackingWide
            font.capitalization: Font.AllUppercase
            color: AppTheme.colourMuted
        }

        Flow
        {
            Layout.fillWidth: true
            spacing: AppMetrics.gapSmall

            Repeater
            {
                model: group.values

                delegate: MobileFilterChip
                {
                    required property string modelData
                    required property int index

                    objectName: "reader_filter_" + group.key + "_" + modelData
                    label: group.labels[index]
                    selected: root.filter[group.key].indexOf(modelData) >= 0

                    onClicked: root.toggled(group.key, modelData)
                }
            }
        }
    }

    FilterGroup
    {
        key: "levels"
        title: qsTr("Level")
        values: Shelf.levels.slice(1)
        labels: Shelf.levelLabels().slice(1)
    }

    FilterGroup
    {
        key: "statuses"
        title: qsTr("Status")
        values: ["new", "reading", "finished"]
        labels: [qsTr("Not started"), qsTr("Reading"), qsTr("Finished")]
    }

    FilterGroup
    {
        key: "storage"
        title: qsTr("Storage")
        values: ["downloaded", "cloud"]
        labels: [qsTr("On this phone"), qsTr("In the cloud")]
    }

    FilterGroup
    {
        key: "formats"
        title: qsTr("Format")
        values: root.formats
        labels: root.formats.map(function(format) { return format.toUpperCase() })
    }

    footer: [
        MobileWideButton
        {
            objectName: "reader_filter_reset"

            label: qsTr("Reset")
            filled: false
            interactive: Shelf.filterCount(root.filter) > 0

            onClicked: root.reset()
        },
        MobileWideButton
        {
            objectName: "reader_filter_show"

            label: root.resultCount === 1 ? qsTr("Show 1 book") : qsTr("Show %1 books").arg(root.resultCount)

            onClicked: root.close()
        }
    ]
}
