import QtQuick
import QtQuick.Layouts

Text
{
    id: root

    property int maximumSize: AppMetrics.sessionWordSize
    property int weight: Font.Medium
    property int gender: -1
    property string verbGroup: ""
    property color baseColour: AppTheme.colourText

    readonly property real widestWordWidth: root.widestWord(root.text, metrics.font)

    function escaped(value)
    {
        return value.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
    }

    function withArticle(word, article)
    {
        if (article.length === 0 || !word.startsWith(article))
        {
            return root.escaped(word)
        }

        return "<font color=\"" + AppTheme.colourMuted + "\">" + root.escaped(article) + "</font>"
            + root.escaped(word.substring(article.length))
    }

    function widestWord(text, font)
    {
        return text.replace(/<[^>]*>/g, " ").split(/\s+/)
            .reduce((Widest, Word) => Math.max(Widest, metrics.advanceWidth(Word)), 0)
    }

    FontMetrics
    {
        id: metrics

        font.family: AppMetrics.fontFamily
        font.weight: root.weight
        font.pixelSize: root.maximumSize
    }

    Layout.fillWidth: true

    font.pixelSize: root.width > 0 && root.widestWordWidth > root.width
        ? Math.max(AppMetrics.sessionTranslationSize, Math.floor(root.maximumSize * root.width / root.widestWordWidth))
        : root.maximumSize
    font.weight: root.weight
    font.family: AppMetrics.fontFamily
    color: AppTheme.verbGroupColours[root.verbGroup] ?? AppTheme.genderColours[root.gender] ?? root.baseColour
    horizontalAlignment: Text.AlignHCenter
    wrapMode: Text.Wrap
}
