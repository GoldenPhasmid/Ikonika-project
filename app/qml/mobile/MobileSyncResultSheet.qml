import QtQuick
import QtQuick.Layouts

MobileBottomSheet
{
    id: root

    heading: qsTr("Database")

    visible: false

    onDismissed: root.close()

    function show(message)
    {
        messageText.text = message
        root.present()
    }

    function close()
    {
        root.hide()
    }

    Text
    {
        id: messageText

        Layout.fillWidth: true

        color: AppTheme.colourSubtle
        font.pixelSize: AppMetrics.fontBody
        font.family: AppMetrics.fontFamily
        wrapMode: Text.WordWrap
    }

    MobileWideButton
    {
        label: qsTr("OK")
        filled: true

        onClicked: root.close()
    }
}
