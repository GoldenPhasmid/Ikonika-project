import QtQuick
import QtQuick.Layouts

ColumnLayout
{
    id: root

    required property string label

    property alias text: field.text
    property alias placeholder: field.placeholderText
    property alias editable: field.enabled
    property alias fieldFocus: field.focus

    readonly property string enteredText: field.displayText

    function replaceText(value)
    {
        const bWasComposing = field.preeditText.length > 0

        if (bWasComposing)
        {
            Qt.inputMethod.commit()
        }

        field.text = value

        if (bWasComposing)
        {
            Qt.inputMethod.reset()
        }
    }

    signal accepted()

    Layout.fillWidth: true
    spacing: AppMetrics.gapSmall

    MobileSectionLabel { text: root.label }

    MobileTextField
    {
        id: field

        Layout.fillWidth: true
        Layout.preferredHeight: 47

        onAccepted: root.accepted()
    }
}
