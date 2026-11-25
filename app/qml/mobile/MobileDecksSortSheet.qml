import QtQuick
import QtQuick.Layouts
import "../Decks.js" as Decks

MobileBottomSheet
{
    id: root

    property string order: Decks.sortOrders[0]
    property bool reversed: false

    signal sortChosen(string order, bool reversed)

    heading: qsTr("Sort decks")
    visible: false

    onDismissed: root.close()

    function open()
    {
        root.present()
    }

    function close()
    {
        root.hide()
    }

    component Choice: MobileSettingsRow
    {
        id: choice

        property bool chosen: false

        DesktopIcon
        {
            visible: choice.chosen
            name: "check"
            width: AppMetrics.fontLabel
            height: width
            color: AppTheme.colourAccent
        }
    }

    ColumnLayout
    {
        Layout.fillWidth: true
        spacing: AppMetrics.gapMedium

        MobileOptionListPanel
        {
            Repeater
            {
                model: Decks.sortOrders

                delegate: Choice
                {
                    required property string modelData
                    required property int index

                    objectName: "decks_sort_" + modelData
                    title: Decks.sortLabels()[index]
                    chosen: root.order === modelData
                    lastRow: index === Decks.sortOrders.length - 1

                    onClicked: root.sortChosen(modelData, root.reversed)
                }
            }
        }

        MobileOptionListPanel
        {
            Repeater
            {
                model: 2

                delegate: Choice
                {
                    required property int index

                    objectName: "decks_sort_direction_" + index
                    title: Decks.sortDirections()[root.order][index]
                    chosen: root.reversed === (index === 1)
                    lastRow: index === 1

                    onClicked: root.sortChosen(root.order, index === 1)
                }
            }
        }
    }
}
