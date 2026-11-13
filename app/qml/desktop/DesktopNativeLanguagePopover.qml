import QtQuick
import QtQuick.Controls.Basic

DesktopPopover
{
    id: root

    required property var store

    heading: qsTr("Your language")

    Repeater
    {
        model: root.store.nativeLanguageCodes

        delegate: DesktopPopoverRow
        {
            required property string modelData
            required property int index

            objectName: "native_language_" + modelData
            text: root.store.nativeLanguageNames[index]
            flagCode: modelData
            showsCheck: true
            selected: index === root.store.nativeLanguageIndex

            onClicked:
            {
                console.info("Native language switched to", modelData)
                root.store.nativeLanguageIndex = index
                root.close()
            }
        }
    }
}
