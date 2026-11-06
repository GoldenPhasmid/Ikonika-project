import QtQuick
import QtQuick.Layouts

MobileBottomSheet
{
    id: root

    property string languageCode: ""
    property string languageName: ""
    property int words: 0

    heading: qsTr("Delete your %1 words?").arg(root.languageName)

    visible: false

    onDismissed: root.close()

    function openFor(entry, count)
    {
        root.languageCode = entry.code
        root.languageName = entry.name
        root.words = count
        root.present()
    }

    function close()
    {
        root.hide()
    }

    Text
    {
        Layout.fillWidth: true

        text: qsTr("%n words and their progress are deleted here and on every device that syncs with your Google account. This cannot be undone.", "", root.words)
        color: AppTheme.colourSubtle
        font.pixelSize: AppMetrics.fontBody
        font.family: AppMetrics.fontFamily
        wrapMode: Text.WordWrap
    }

    MobileButtonPair
    {
        leftLabel: qsTr("Cancel")
        rightLabel: qsTr("Delete")
        rightDestructive: true

        onLeftClicked: root.close()
        onRightClicked:
        {
            root.close()
            AppSyncController.deleteLanguage(root.languageCode)
        }
    }
}
