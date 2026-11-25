import QtQuick
import QtQuick.Layouts

MobileBottomSheet
{
    id: root

    property bool coloursShown: false
    property bool descriptionShown: false
    property bool focusWhenShown: false
    property int colour: 0
    property var saved: null

    visible: false

    onDismissed: root.close()

    onRevealChanged:
    {
        if (root.reveal < 1 || !root.focusWhenShown)
        {
            return
        }

        root.focusWhenShown = false
        nameField.forceActiveFocus()
        nameField.selectAll()
    }

    function open(title, text, placeholder, coloursShown, colour, onSaved)
    {
        root.heading = title
        nameField.text = text
        nameField.placeholderText = placeholder
        root.coloursShown = coloursShown
        root.descriptionShown = false
        root.colour = colour
        root.saved = onSaved
        root.focusWhenShown = true
        root.present()
    }

    function openDeck(title, name, description, onSaved)
    {
        root.open(title, name, qsTr("Deck name"), false, 0, onSaved)
        descriptionField.text = description
        root.descriptionShown = true
    }

    function close()
    {
        root.forceActiveFocus()
        Qt.inputMethod.hide()
        root.hide()
    }

    function save()
    {
        const name = nameField.text.trim()

        if (name.length === 0)
        {
            return
        }

        const handler = root.saved

        root.saved = null
        root.close()

        if (handler)
        {
            handler(name, root.descriptionShown ? descriptionField.text.trim() : root.colour)
        }
    }

    MobileTextField
    {
        id: nameField

        objectName: "name_sheet_field"

        Layout.fillWidth: true
        Layout.preferredHeight: AppMetrics.buttonHeightLarge

        onAccepted: root.descriptionShown ? descriptionField.forceActiveFocus() : root.save()
    }

    MobileTextField
    {
        id: descriptionField

        objectName: "name_sheet_description"

        Layout.fillWidth: true
        Layout.preferredHeight: AppMetrics.buttonHeightLarge
        visible: root.descriptionShown
        placeholderText: qsTr("Description")

        onAccepted: root.save()
    }

    Flow
    {
        Layout.fillWidth: true
        visible: root.coloursShown
        spacing: AppMetrics.gapMedium

        Repeater
        {
            model: AppTheme.coverColours.length

            delegate: Rectangle
            {
                required property int index

                width: AppMetrics.buttonHeightLarge - AppMetrics.gapSmall
                height: width
                radius: width / 2
                color: AppTheme.coverColours[index]
                border.width: root.colour === index ? AppMetrics.borderWidth * 2 : 0
                border.color: AppTheme.colourText

                Accessible.role: Accessible.RadioButton
                Accessible.name: qsTr("Colour %1").arg(index + 1)
                Accessible.checked: root.colour === index

                TapHandler { onTapped: root.colour = index }
            }
        }
    }

    MobileWideButton
    {
        objectName: "name_sheet_save"

        label: qsTr("Save")
        interactive: nameField.text.trim().length > 0

        onClicked: root.save()
    }
}
