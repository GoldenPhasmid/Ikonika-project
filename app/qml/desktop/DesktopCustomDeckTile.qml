import QtQuick
import QtQuick.Controls.Basic

DesktopDeckTile
{
    id: root

    required property var deck

    property color categoryTint: AppTheme.colourCoverMuted

    signal editRequested()
    signal deleteRequested(string deckId, string name)

    objectName: "user_deck_" + root.deck.id

    title: root.deck.name
    iconName: root.deck.icon
    tint: root.categoryTint
    wordCount: root.deck.wordCount
    learningCount: root.deck.learningCount
    learnedCount: root.deck.learnedCount

    TapHandler
    {
        acceptedButtons: Qt.RightButton
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad

        onTapped: function(point) { menu.popup(root, point.position.x, point.position.y) }
    }

    DesktopMenu
    {
        id: menu

        DesktopMenuItem
        {
            text: qsTr("Edit deck")
            iconName: "edit"

            onTriggered: root.editRequested()
        }

        DesktopMenuItem
        {
            text: qsTr("Remove from category")
            enabled: root.deck.categoryId.length > 0

            onTriggered: UserDecksController.moveDeck(root.deck.id, "")
        }

        Instantiator
        {
            model: UserDecksController.categories.filter(category => category.id !== root.deck.categoryId)

            delegate: DesktopMenuItem
            {
                required property var modelData

                text: qsTr("Move to %1").arg(modelData.name)

                onTriggered: UserDecksController.moveDeck(root.deck.id, modelData.id)
            }

            onObjectAdded: function(index, object) { menu.insertItem(index + 2, object) }
            onObjectRemoved: function(index, object) { menu.removeItem(object) }
        }

        DesktopMenuItem
        {
            text: qsTr("Delete deck")
            iconName: "trash"
            destructive: true

            onTriggered: root.deleteRequested(root.deck.id, root.deck.name)
        }
    }
}
