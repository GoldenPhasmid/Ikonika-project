import QtQuick

TapHandler
{
    id: root

    required property Item editor
    property bool armed: false

    onPressedChanged:
    {
        if (root.pressed)
        {
            root.armed = root.editor.activeFocus && !root.editor.readOnly && !Qt.inputMethod.visible
        }
    }

    onTapped:
    {
        if (!root.armed)
        {
            return
        }

        root.armed = false

        console.info("Text field tapped while focused without a keyboard, refocusing it to raise the keyboard")

        root.editor.focus = false
        root.editor.forceActiveFocus()
        Qt.inputMethod.show()
    }
}
