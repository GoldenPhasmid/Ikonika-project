import QtQuick

Item
{
    id: root

    required property Item editor

    Timer
    {
        id: releaseDelay

        interval: AppMetrics.keyboardReleaseDelay

        onTriggered:
        {
            if (Qt.inputMethod.visible || !root.editor.activeFocus)
            {
                return
            }

            console.info("Keyboard stayed hidden, releasing focus from the text field")

            Qt.inputMethod.reset()
            root.editor.focus = false
        }
    }

    Connections
    {
        target: Qt.inputMethod

        function onVisibleChanged()
        {
            if (Qt.inputMethod.visible)
            {
                releaseDelay.stop()

                return
            }

            if (root.editor.activeFocus)
            {
                releaseDelay.restart()
            }
        }
    }
}
