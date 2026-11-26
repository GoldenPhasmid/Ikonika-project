import QtQuick
import QtQuick.Layouts

MobileBottomSheet
{
    id: root

    required property var pane
    property bool showLookupActions: true

    function run(action)
    {
        action()
        root.dismiss()
    }

    heading: root.pane.card ? root.pane.card.word : ""
    maxHeightFraction: 0.8

    MobileOptionListPanel
    {
        MobileSettingsRow
        {
            objectName: "card_action_dictionary"

            visible: root.showLookupActions
            title: root.height > root.width ? qsTr("Open in Dictionary") : qsTr("Open in Words")

            onClicked: root.run(root.pane.openInWords)

            MobileSettingsChevron {}
        }

        MobileSettingsRow
        {
            objectName: "card_action_web"

            visible: root.showLookupActions
            title: qsTr("Search the web")
            lastRow: !root.pane.canComplete && !root.pane.canReset && !root.pane.canRemove

            onClicked: root.run(root.pane.searchWeb)

            MobileSettingsChevron {}
        }

        MobileSettingsRow
        {
            objectName: "card_action_complete"

            visible: root.pane.canComplete
            title: qsTr("Mark as learned")
            lastRow: !root.pane.canReset && !root.pane.canRemove

            onClicked: root.run(root.pane.completeProgress)
        }

        MobileSettingsRow
        {
            objectName: "card_action_reset"

            visible: root.pane.canReset
            title: qsTr("Reset progress")
            lastRow: !root.pane.canRemove

            onClicked: root.run(root.pane.resetProgress)
        }

        MobileSettingsRow
        {
            objectName: "card_action_remove"

            visible: root.pane.canRemove
            title: qsTr("Remove from My Vocabulary")
            destructive: true
            holdToConfirm: true
            lastRow: true

            onClicked: root.run(root.pane.remove)
        }
    }
}
