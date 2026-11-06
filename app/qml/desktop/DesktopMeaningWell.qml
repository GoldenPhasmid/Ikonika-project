import QtQuick
import QtQuick.Layouts

DesktopRevealWell
{
    id: root

    property var controller: null
    property string answer: ""

    signal wordClicked(string word, string sentence)

    DesktopLabel
    {
        Layout.fillWidth: true
        text: root.answer
        color: root.controller && !root.controller.showsTargetLanguage
            ? AppTheme.verbGroupColours[root.controller.targetVerbGroup] ?? AppTheme.colourText : AppTheme.colourText
        font.pixelSize: AppMetrics.sessionTranslationSize
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        maximumLineCount: AppMetrics.answerLineCount
        elide: Text.ElideRight
    }

    DesktopRevealDetails
    {
        controller: root.revealed ? root.controller : null

        onWordClicked: function(word, sentence) { root.wordClicked(word, sentence) }
    }
}
