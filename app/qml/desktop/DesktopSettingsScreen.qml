import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item
{
    id: root

    required property var store
    property var triageController: null

    signal placementRequested()

    property string category: "account"

    readonly property var categories: [
        { key: "account", title: qsTr("Account") },
        { key: "general", title: qsTr("General") },
        { key: "training", title: qsTr("Training") },
        { key: "review", title: qsTr("Review") },
        { key: "sound", title: qsTr("Sound") },
        { key: "reader", title: qsTr("Reader") },
        { key: "reminders", title: qsTr("Reminders") },
        { key: "about", title: qsTr("About") }
    ]

    readonly property int categoryIndex: root.categories.findIndex(Entry => Entry.key === root.category)

    function open(key)
    {
        root.category = key
        page.contentY = 0
    }

    function step(delta)
    {
        const next = Math.max(0, Math.min(root.categories.length - 1, root.categoryIndex + delta))

        root.open(root.categories[next].key)
    }

    DesktopSidePane
    {
        id: categoryPane

        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        widthKey: "settingsCategories"
        defaultWidth: AppMetrics.categoryPaneWidth
        spacing: AppMetrics.borderWidth

        Accessible.role: Accessible.PageTabList
        Accessible.name: qsTr("Settings categories")

        Repeater
        {
            model: root.categories

            delegate: ItemDelegate
            {
                id: categoryItem

                required property var modelData

                readonly property bool selected: root.category === modelData.key

                objectName: "settings_" + modelData.key

                Layout.fillWidth: true
                implicitHeight: AppMetrics.navItemHeight
                leftPadding: AppMetrics.gapMedium
                rightPadding: AppMetrics.gapSmall
                hoverEnabled: true
                focusPolicy: Qt.StrongFocus
                text: modelData.title

                Accessible.role: Accessible.PageTab
                Accessible.name: modelData.title
                Accessible.selected: categoryItem.selected

                Keys.onDownPressed: root.step(1)
                Keys.onUpPressed: root.step(-1)

                onClicked: root.open(modelData.key)

                contentItem: DesktopLabel
                {
                    text: categoryItem.modelData.title
                    font.weight: categoryItem.selected ? Font.DemiBold : Font.Medium
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }

                background: DesktopSelectionBackground
                {
                    selected: categoryItem.selected
                    hovered: categoryItem.hovered
                    pressed: categoryItem.down
                    focused: categoryItem.visualFocus
                }

                HoverHandler
                {
                    cursorShape: Qt.PointingHandCursor
                }
            }
        }

        Item { Layout.fillHeight: true }

        Flickable
        {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(updateCards.implicitHeight, categoryPane.height * 0.45)
            contentHeight: updateCards.implicitHeight
            clip: true
            ScrollBar.vertical: ScrollBar {}

            ColumnLayout
            {
                id: updateCards

                width: parent.width
                spacing: AppMetrics.gapSmall

                DesktopAppUpdateCard
                {
                    objectName: "settings_app_update"
                }

                Repeater
                {
                    model: AppCatalog.packageUpdates

                    delegate: DesktopPackageUpdateCard
                    {
                        required property var modelData

                        objectName: "settings_" + modelData.id + "_update"
                        update: modelData
                    }
                }
            }
        }
    }

    DesktopPage
    {
        id: page

        anchors.left: categoryPane.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        maximumWidth: AppMetrics.contentWidthNarrow

        DesktopSettingsAccount
        {
            id: accountPage

            visible: root.category === "account"
            store: root.store
            triage: root.triageController

            onVisibleChanged: if (visible) accountPage.refresh()
            onPlacementRequested: root.placementRequested()
        }

        DesktopSettingsGeneral
        {
            visible: root.category === "general"
            store: root.store
        }

        DesktopSettingsSessionStages
        {
            visible: root.category === "training"
            store: root.store
            session: root.store.training
        }

        DesktopSettingsSessionStages
        {
            visible: root.category === "review"
            store: root.store
            session: root.store.review
        }

        DesktopSettingsSound
        {
            visible: root.category === "sound"
            store: root.store
        }

        DesktopSettingsReader
        {
            visible: root.category === "reader"
        }

        DesktopSettingsReminders
        {
            visible: root.category === "reminders"
        }

        DesktopSettingsAbout
        {
            visible: root.category === "about"
        }
    }
}
