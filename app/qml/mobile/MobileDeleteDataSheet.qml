import QtQuick
import QtQuick.Layouts

MobileBottomSheet
{
    id: root

    readonly property string confirmWord: "DELETE"
    readonly property bool armed: confirmField.text.toUpperCase() === root.confirmWord

    heading: qsTr("Delete everything")

    visible: false

    onDismissed: root.close()

    function openFresh()
    {
        confirmField.text = ""
        root.present()
    }

    function close()
    {
        root.hide()
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

    Text
    {
        Layout.fillWidth: true

        text: qsTr("Delete everything. Deletes both local and cloud user information. Do "
            + "you wish to proceed?")
        color: AppTheme.colourSubtle
        font.pixelSize: AppMetrics.fontBody
        font.family: AppMetrics.fontFamily
        wrapMode: Text.WordWrap
    }

    ColumnLayout
    {
        Layout.fillWidth: true
        spacing: AppMetrics.gapSmall

        MobileSectionLabel
        {
            text: qsTr("Type %1 to confirm").arg(root.confirmWord)
        }

        MobileTextField
        {
            id: confirmField

            focus: root.visible

            Layout.fillWidth: true
            Layout.preferredHeight: 47

            placeholderText: root.confirmWord

            onAccepted: root.commit()
        }
    }

    MobileButtonPair
    {
        leftLabel: qsTr("Cancel")
        rightLabel: qsTr("Delete")
        rightEnabled: root.armed
        rightDestructive: true

        onLeftClicked: root.close()
        onRightClicked: root.commit()
    }
}
