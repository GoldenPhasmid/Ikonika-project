import QtQuick
import QtQuick.Layouts

Rectangle
{
    id: root

    required property string activeTab

    readonly property var tabs: [
        { key: "today", label: qsTr("Today"), icon: "nav_today" },
        { key: "dictionary", label: qsTr("Words"), icon: "nav_words" },
        { key: "reader", label: qsTr("Reader"), icon: "nav_reader" },
        DecksController.available ? { key: "decks", label: qsTr("Decks"), icon: "nav_decks" } : null,
        { key: "vocabulary", label: qsTr("Vocabulary"), icon: "nav_vocabulary" }
    ].filter(Tab => Tab !== null)

    signal tabSelected(string key)

    Layout.fillWidth: true
    Layout.preferredHeight: 82

    color: AppTheme.colourPanel

    Rectangle
    {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: AppMetrics.borderWidth
        color: AppTheme.colourBorder
    }

    RowLayout
    {
        anchors.fill: parent
        anchors.topMargin: 12
        anchors.bottomMargin: 12
        spacing: 0

        Repeater
        {
            model: root.tabs

            delegate: MobileBottomTab
            {
                required property var modelData

                objectName: "tab_" + modelData.key
                label: modelData.label
                active: modelData.key === root.activeTab

                iconName: modelData.icon

                onActivated: root.tabSelected(modelData.key)
            }
        }
    }
}
