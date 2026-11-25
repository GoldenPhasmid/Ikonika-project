import QtQuick
import QtQuick.Layouts

MobileBottomSheet
{
    id: root

    property var deck: ({})
    property bool builtin: false
    property string categoryName: ""

    signal editRequested()
    signal categoryRequested()
    signal dailyLearningChosen(bool dailyLearning)
    signal importRequested()
    signal exportRequested()
    signal deleteRequested()

    function run(action)
    {
        action()
        root.dismiss()
    }

    heading: root.deck.name ?? ""
    maxHeightFraction: 0.8

    MobileOptionListPanel
    {
        MobileSettingsRow
        {
            objectName: "deck_action_edit"

            visible: !root.builtin
            title: qsTr("Edit deck")

            onClicked: root.run(root.editRequested)

            MobileSettingsChevron {}
        }

        MobileSettingsRow
        {
            objectName: "deck_action_category"

            visible: !root.builtin
            title: qsTr("Category")

            onClicked: root.run(root.categoryRequested)

            RowLayout
            {
                spacing: AppMetrics.gapSmall

                MobileSettingsValue
                {
                    text: root.categoryName.length > 0 ? root.categoryName : qsTr("No category")
                }

                MobileSettingsChevron {}
            }
        }

        MobileSettingsRow
        {
            objectName: "deck_action_daily_learning"

            visible: !root.builtin
            title: qsTr("Daily learning")

            onClicked: root.dailyLearningChosen(!(root.deck.dailyLearning ?? true))

            MobileSettingsToggle
            {
                checked: root.deck.dailyLearning ?? true

                onToggled: root.dailyLearningChosen(!checked)
            }
        }

        MobileSettingsRow
        {
            objectName: "deck_action_import"

            visible: !root.builtin
            title: qsTr("Import")

            onClicked: root.run(root.importRequested)
        }

        MobileSettingsRow
        {
            objectName: "deck_action_export"

            title: qsTr("Export")
            lastRow: root.builtin

            onClicked: root.run(root.exportRequested)
        }

        MobileSettingsRow
        {
            objectName: "deck_action_delete"

            visible: !root.builtin
            title: qsTr("Delete deck")
            destructive: true
            holdToConfirm: true
            lastRow: true

            onClicked: root.run(root.deleteRequested)
        }
    }
}
