import QtQuick
import QtQuick.Layouts

DesktopPromptLabel
{
    id: root

    objectName: "sentence_text"

    property string sentence: ""
    property int gapStart: -1
    property int gapLength: 0
    property bool interactive: true

    signal wordClicked(string word, string sentence, rect wordRect)

    property point pressPosition

    readonly property int hoveredWord: root.hoveredLink.startsWith("word:") ? parseInt(root.hoveredLink.substring(5)) : -1

    readonly property var parsed: root.parse(root.sentence, root.gapStart, root.gapLength, root.hoveredWord)

    function escapeHtml(value)
    {
        return value.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
    }

    function parse(sentence, gapStart, gapLength, hovered)
    {
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

            const inGap = gapLength > 0 && start < gapStart + gapLength && end > gapStart

            if (letterPattern.test(word) && root.interactive)
            {
                const colour = inGap ? AppTheme.colourAccent : AppTheme.colourText
                const decoration = words.length === hovered ? "underline" : "none"

                markup += "<a href=\"word:" + words.length + "\" style=\"color:" + colour + ";text-decoration:" + decoration + ";\">"
                    + root.escapeHtml(word) + "</a>"

                words.push(word)
            }
            else if (inGap)
            {
                markup += "<span style=\"color:" + AppTheme.colourAccent + ";\">" + root.escapeHtml(word) + "</span>"
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

    Layout.fillHeight: false

    maximumSize: AppMetrics.sessionTranslationSize
    text: root.parsed.markup
    textFormat: Text.RichText
    fontSizeMode: Text.FixedSize
    font.weight: Font.Medium

    Accessible.name: root.sentence

    function linkRect(link)
    {
        const position = root.pressPosition
        let left = position.x
        let right = position.x
        let top = position.y
        let bottom = position.y

        while (left > 0 && root.linkAt(left - 1, position.y) === link)
        {
            --left
        }
        while (right < root.width && root.linkAt(right, position.y) === link)
        {
            ++right
        }
        while (top > 0 && root.linkAt(position.x, top - 1) === link)
        {
            --top
        }
        while (bottom < root.height && root.linkAt(position.x, bottom) === link)
        {
            ++bottom
        }

        return root.mapToItem(null, Qt.rect(left, top, right - left, bottom - top))
    }

    onLinkActivated: function(link)
    {
        if (!link.startsWith("word:"))
        {
            return
        }

        const word = root.parsed.words[parseInt(link.substring(5))] || ""

        if (word.length > 0)
        {
            root.wordClicked(word, root.sentence, root.linkRect(link))
        }
    }

    PointHandler
    {
        enabled: root.interactive
        onActiveChanged: if (active) root.pressPosition = point.position
    }

    HoverHandler
    {
        cursorShape: root.hoveredWord >= 0 ? Qt.PointingHandCursor : Qt.ArrowCursor
    }
}
