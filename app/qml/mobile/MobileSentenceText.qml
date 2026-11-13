import QtQuick
import QtQuick.Layouts

Text
{
    id: root

    property string sentence: ""
    property int gapStart: -1
    property int gapLength: 0
    property bool holdEnabled: true
    property bool tapEnabled: false

    signal wordHeld(string word)
    signal wordTapped(string word, string sentence)

    readonly property var parsed: root.parse(root.sentence, root.gapStart, root.gapLength,
                                             AppTheme.colourText, AppTheme.colourAccent)

    function escapeHtml(value)
    {
        return value.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
    }

    function parse(sentence, gapStart, gapLength, textColour, gapColour)
    {
        sentence = sentence.replace(/ (?=[!?:;»])/g, " ").replace(/« /g, "« ")

        const wordPattern = /[A-Za-zÀ-ÿŒœŸ0-9'’\-]+/g
        const letterPattern = /[A-Za-zÀ-ÿŒœŸ]/
        const words = []

        let markup = ""
        let cursor = 0
        let match = wordPattern.exec(sentence)

        while (match !== null)
        {
            const word = match[0]
            const start = match.index
            const end = start + word.length

            markup += root.escapeHtml(sentence.substring(cursor, start))

            if (letterPattern.test(word))
            {
                const inGap = gapLength > 0 && start < gapStart + gapLength && end > gapStart
                const style = inGap ? "color:" + gapColour + ";"
                                    : "color:" + textColour + ";text-decoration:none;"

                markup += "<a href=\"word:" + words.length + "\" style=\"" + style + "\">"
                    + root.escapeHtml(word) + "</a>"

                words.push(word)
            }
            else
            {
                markup += root.escapeHtml(word)
            }

            cursor = end
            match = wordPattern.exec(sentence)
        }

        markup += root.escapeHtml(sentence.substring(cursor))

        return { markup: markup, words: words }
    }

    function wordAt(x, y)
    {
        const link = root.linkAt(x, y)

        if (!link.startsWith("word:"))
        {
            return ""
        }

        return root.parsed.words[parseInt(link.substring(5))] || ""
    }

    Layout.fillWidth: true

    text: root.parsed.markup
    textFormat: Text.RichText
    font.pixelSize: AppMetrics.sessionTranslationSize
    font.weight: Font.DemiBold
    font.family: AppMetrics.fontFamily
    color: AppTheme.colourText
    horizontalAlignment: Text.AlignHCenter
    wrapMode: Text.WordWrap

    TapHandler
    {
        enabled: root.tapEnabled

        onTapped:
        {
            const word = root.wordAt(point.position.x, point.position.y)

            if (word.length > 0)
            {
                root.wordTapped(word, root.sentence)
            }
        }
    }

    TapHandler
    {
        enabled: root.holdEnabled

        onLongPressed:
        {
            const word = root.wordAt(point.position.x, point.position.y)

            if (word.length > 0)
            {
                root.wordHeld(word)
            }
        }
    }
}
