import QtQuick
import QtQuick.Layouts
import "MobileIcons.js" as Icons

Rectangle
{
    id: root

    required property string activeTab

    readonly property var tabs: [
        {
            key: "today", label: qsTr("Today"),
            icon: Icons.Icons.tab_today.path,
            iconMinX: Icons.Icons.tab_today.minX, iconMinY: Icons.Icons.tab_today.minY,
            iconWidth: Icons.Icons.tab_today.width, iconHeight: Icons.Icons.tab_today.height
        },
        {
            key: "dictionary", label: qsTr("Words"),
            icon: Icons.Icons.tab_dictionary.path,
            iconMinX: Icons.Icons.tab_dictionary.minX, iconMinY: Icons.Icons.tab_dictionary.minY,
            iconWidth: Icons.Icons.tab_dictionary.width, iconHeight: Icons.Icons.tab_dictionary.height
        },
        Qt.platform.os !== "ios" ? {
            key: "reader", label: qsTr("Reader"),
            icon: Icons.Icons.tab_reader.path,
            iconMinX: Icons.Icons.tab_reader.minX, iconMinY: Icons.Icons.tab_reader.minY,
            iconWidth: Icons.Icons.tab_reader.width, iconHeight: Icons.Icons.tab_reader.height
        } : null,
        DecksController.available ? {
            key: "decks", label: qsTr("Decks"),
            icon: Icons.Icons.tab_decks.path,
            iconMinX: Icons.Icons.tab_decks.minX, iconMinY: Icons.Icons.tab_decks.minY,
            iconWidth: Icons.Icons.tab_decks.width, iconHeight: Icons.Icons.tab_decks.height
        } : null,
        {
            key: "vocabulary", label: qsTr("Vocabulary"),
            icon: Icons.Icons.tab_vocabulary.path,
            iconMinX: Icons.Icons.tab_vocabulary.minX, iconMinY: Icons.Icons.tab_vocabulary.minY,
            iconWidth: Icons.Icons.tab_vocabulary.width, iconHeight: Icons.Icons.tab_vocabulary.height
        }
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

                icon: modelData.icon || ""
                iconMinX: modelData.iconMinX || 0
                iconMinY: modelData.iconMinY || 0
                iconWidth: modelData.iconWidth || 1
                iconHeight: modelData.iconHeight || 1

                onActivated: root.tabSelected(modelData.key)
            }
        }
    }
}
