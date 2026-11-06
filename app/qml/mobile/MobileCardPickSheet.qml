import QtQuick
import QtQuick.Layouts

MobileBottomSheet
{
    id: root

    heading: qsTr("Edit card")

    property var cards: []

    signal chosen(var card)

    MobileOptionListPanel
    {
        Repeater
        {
            model: root.cards

            delegate: MobileSettingsRow
            {
                id: cardRow

                required property var modelData
                required property int index

                title: cardRow.modelData.translation
                lastRow: cardRow.index === root.cards.length - 1

                onClicked: root.chosen(cardRow.modelData)

                LearnedMark
                {
                    stage: cardRow.modelData.stage
                    completed: cardRow.modelData.completed
                }
            }
        }
    }
}
