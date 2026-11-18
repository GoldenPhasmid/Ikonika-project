import QtQuick
import QtQuick.Layouts

MobileBottomSheet
{
    id: root

    property bool coloursShown: false
    property int colour: 0
    property var saved: null

    visible: false

    onDismissed: root.close()

    function open(title, text, placeholder, coloursShown, colour, onSaved)
    {
        root.heading = title
        nameField.text = text
        nameField.placeholderText = placeholder
        root.coloursShown = coloursShown
        root.colour = colour
        root.saved = onSaved
        root.present()
        nameField.forceActiveFocus()
        nameField.selectAll()
    }

    function close()
    {
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
            handler(name, root.colour)
        }
    }

    MobileTextField
    {
        id: nameField

        objectName: "name_sheet_field"

        Layout.fillWidth: true
        Layout.preferredHeight: AppMetrics.buttonHeightLarge

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
