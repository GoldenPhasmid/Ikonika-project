import QtQuick
import QtQuick.Layouts

Rectangle
{
    id: root

    required property string activeTab
    property string headerTitle: qsTr("Ikonika")
    property bool headerTitleTappable: false
    property bool showBack: false
    property string trailingIcon: ""
    property bool showOptions: false
    property bool showSearch: false
    property bool showSettings: false
    property bool showSearchField: false
    property bool showTabBar: true
    property bool showHeader: true
    property real contentMaximumWidth: Number.POSITIVE_INFINITY

    property alias headerStatus: header.status
    property alias searchFieldText: header.searchFieldText
    property alias searchFieldPreeditText: header.searchFieldPreeditText

    signal tabSelected(string key)
    signal backRequested()
    signal trailingActivated()
    signal optionsRequested()
    signal searchRequested()
    signal settingsRequested()
    signal searchFieldAccepted()
    signal headerTitleTapped()

    default property alias content: contentArea.data

    function focusSearchField()
    {
        header.focusSearchField()
    }

    color: AppTheme.colourPanel

    ColumnLayout
    {
        anchors.fill: parent
        spacing: 0

        MobileHeader
        {
            id: header

            Layout.topMargin: root.SafeArea.margins.top
            visible: root.showHeader

            title: root.headerTitle
            showBack: root.showBack
            trailingIcon: root.trailingIcon
            showOptions: root.showOptions
            showSearch: root.showSearch
            showSettings: root.showSettings
            showSearchField: root.showSearchField
            titleTappable: root.headerTitleTappable
            contentMaximumWidth: root.contentMaximumWidth

            onBackRequested: root.backRequested()
            onTrailingActivated: root.trailingActivated()
            onOptionsRequested: root.optionsRequested()
            onSearchRequested: root.searchRequested()
            onSettingsRequested: root.settingsRequested()
            onSearchFieldAccepted: root.searchFieldAccepted()
            onTitleTapped: root.headerTitleTapped()
        }

        Item
        {
            id: contentArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.maximumWidth: root.contentMaximumWidth
            Layout.alignment: Qt.AlignHCenter
        }

        MobileBottomTabBar
        {
            activeTab: root.activeTab
            visible: root.showTabBar
            Layout.bottomMargin: root.SafeArea.margins.bottom

            onTabSelected: function(key) { root.tabSelected(key) }
        }
    }
}
