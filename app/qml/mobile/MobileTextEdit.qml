import QtQuick

TextEdit
{
    id: root

    property string placeholderText: ""
    property color placeholderTextColor: AppTheme.colourMuted

    wrapMode: TextEdit.WordWrap

    Text
    {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        visible: root.placeholderText.length > 0
            && root.text.length === 0 && root.preeditText.length === 0
        text: root.placeholderText
        font: root.font
        color: root.placeholderTextColor
        wrapMode: Text.WordWrap
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
