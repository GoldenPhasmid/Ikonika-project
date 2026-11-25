import QtQuick
import QtQuick.Layouts

DesktopDialog
{
    id: root

    property var action: null
    property bool creating: false

    readonly property string name: nameField.text.trim()

    title: root.creating ? qsTr("New deck") : qsTr("Edit deck")

    function openFor(deck, action)
    {
        root.creating = !deck
        root.action = action
        nameField.text = deck?.name ?? ""
        descriptionField.text = deck?.description ?? ""
        root.open()
    }

    function confirm()
    {
        if (root.name.length === 0)
        {
            return
        }

        const action = root.action
        const name = root.name
        const description = descriptionField.text.trim()

        root.close()

        if (action)
        {
            action(name, description)
        }
    }

    onOpened:
    {
        nameField.forceActiveFocus()
        nameField.selectAll()
    }

    ColumnLayout
    {
        Layout.fillWidth: true
        spacing: AppMetrics.gapMedium

        DesktopFormField
        {
            label: qsTr("Name")

            DesktopTextField
            {
                id: nameField

                objectName: "deck_edit_name"

                Layout.fillWidth: true
                maximumLength: 60

                Accessible.name: qsTr("Name")

                Keys.onReturnPressed: root.confirm()
                Keys.onEnterPressed: root.confirm()
            }
        }

        DesktopFormField
        {
            label: qsTr("Description")

            DesktopTextArea
            {
                id: descriptionField

                objectName: "deck_edit_description"

                accessibleName: qsTr("Description")
                submitOnReturn: true

                onAccepted: root.confirm()
            }
        }
    }

    footer: [
        DesktopButton
        {
            large: true
            text: qsTr("Cancel")

            onClicked: root.close()
        },
        DesktopButton
        {
            objectName: "deck_edit_confirm"

            large: true
            tone: "primary"
            enabled: root.name.length > 0
            text: root.creating ? qsTr("Create") : qsTr("Save")

            onClicked: root.confirm()
        }
    ]
}
