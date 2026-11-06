import QtQuick
import QtQuick.Layouts

DesktopDialog
{
    id: root

    readonly property string confirmWord: "DELETE"
    readonly property bool armed: confirmField.text.trim().toUpperCase() === root.confirmWord

    title: qsTr("Delete everything?")
    message: qsTr("Your cards and progress are deleted on this computer and in the cloud. This cannot be undone.")
    dismissable: false

    function openFresh()
    {
        confirmField.text = ""
        root.open()
        Qt.callLater(function() { confirmField.forceActiveFocus() })
    }

    function commit()
    {
        if (!root.armed)
        {
            return
        }

        AppSyncController.eraseAllData()
        root.close()
    }

    DesktopFormField
    {
        label: qsTr("Type %1 to confirm").arg(root.confirmWord)

        DesktopTextField
        {
            id: confirmField

            Layout.fillWidth: true
            placeholderText: root.confirmWord

            onAccepted: root.commit()
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
            large: true
            tone: "danger"
            text: qsTr("Delete everything")
            enabled: root.armed

            onClicked: root.commit()
        }
    ]
}
