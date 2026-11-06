import QtQuick

TextInput
{
    id: root

    property string placeholderText: ""
    property color placeholderTextColor: AppTheme.colourMuted

    Text
    {
        anchors.fill: parent
        visible: root.placeholderText.length > 0
            && root.text.length === 0 && root.preeditText.length === 0
        text: root.placeholderText
        font: root.font
        color: root.placeholderTextColor
        verticalAlignment: root.verticalAlignment
        horizontalAlignment: root.horizontalAlignment
    }

    Connections
    {
        target: Qt.inputMethod

        function onVisibleChanged()
        {
            if (!Qt.inputMethod.visible && root.activeFocus)
            {
                Qt.inputMethod.reset()
                root.focus = false
            }
        }
    }
}
