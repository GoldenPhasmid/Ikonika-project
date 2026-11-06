import QtQuick
import QtQuick.Layouts

ColumnLayout
{
    id: root

    property alias searchText: searchField.text
    readonly property alias searchPreeditText: searchField.preeditText
    property string searchPlaceholder: qsTr("Search...")

    property string filterLabel: qsTr("All")
    property bool showFilter: true
    property string sortLabel: ""

    property string counterText: ""

    default property alias extras: extrasSlot.data

    signal searchAccepted()
    signal filterRequested()
    signal sortRequested()

    Layout.fillWidth: true
    spacing: AppMetrics.gapWide

    MobileSearchField
    {
        id: searchField

        placeholderText: root.searchPlaceholder

        onAccepted: root.searchAccepted()
    }

    ColumnLayout
    {
        id: extrasSlot

        Layout.fillWidth: true
        visible: implicitHeight > 0
        spacing: 0
    }

    RowLayout
    {
        Layout.fillWidth: true
        spacing: AppMetrics.gapSmall

        MobileSettingsSelect
        {
            visible: root.showFilter
            label: root.filterLabel

            onClicked: root.filterRequested()
        }

        MobileSettingsSelect
        {
            label: root.sortLabel

            onClicked: root.sortRequested()
        }

        Item { Layout.fillWidth: true }

        Text
        {
            visible: root.counterText.length > 0
            text: root.counterText
            font.pixelSize: AppMetrics.fontSmall
            font.family: AppMetrics.fontFamily
            color: AppTheme.colourMuted
        }
    }
}
