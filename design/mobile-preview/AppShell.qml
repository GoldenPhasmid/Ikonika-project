import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    property string activeTab: ""
    property string headerTitle: "Ikonika"
    property bool showBack: false
    property string trailingIcon: ""
    property bool showSearch: false
    property bool showSearchField: false
    property bool showTabBar: false

    property alias searchFieldText: header.searchFieldText

    signal backRequested()
    signal trailingActivated()
    signal searchRequested()
    signal searchFieldAccepted()

    default property alias content: contentArea.data

    color: Theme.panel

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        AppHeader {
            id: header

            Layout.topMargin: root.SafeArea.margins.top

            title: root.headerTitle
            showBack: root.showBack
            trailingIcon: root.trailingIcon
            showSearch: root.showSearch
            showSearchField: root.showSearchField

            onBackRequested: root.backRequested()
            onTrailingActivated: root.trailingActivated()
            onSearchRequested: root.searchRequested()
            onSearchFieldAccepted: root.searchFieldAccepted()
        }

        Item {
            id: contentArea
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        BottomTabBar {
            activeTab: root.activeTab
            visible: root.showTabBar

            Layout.bottomMargin: root.SafeArea.margins.bottom
        }
    }
}
