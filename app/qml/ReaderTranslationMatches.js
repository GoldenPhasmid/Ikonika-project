.pragma library

function wordCount(text)
{
    return text.split(/\s+/).filter(part => part.length > 0).length
}

function findMatches(results)
{
    return results.filter(Row => Row.exact)
}

function translationStem(phrase)
{
    if (phrase.length > 5)
    {
        return phrase.slice(0, -2)
    }

    return phrase.length > 3 ? phrase.slice(0, -1) : phrase
}

function supportedBy(row, contextText)
{
    const words = contextText.split(/[\s.,;:!?"()\[\]«»—–-]+/).filter(Word => Word.length > 0)

    return (row.translation || "").split(", ").some(function(Phrase)
    {
        const phrase = Phrase.trim().toLowerCase().replace(/^(to|a|an|the)\s+/, "")

        if (phrase.length === 0)
        {
            return false
        }

        if (phrase.indexOf(" ") >= 0)
        {
            return contextText.indexOf(phrase) >= 0
        }

        const stem = translationStem(phrase)

        return words.some(Word => Word === phrase || Word.startsWith(stem))
    })
}

function rankMatches(rows, contextTranslation)
{
    const contextText = contextTranslation.toLowerCase()

    if (rows.length < 2 || contextText.length === 0)
    {
        return rows
    }

    const supported = rows.filter(Row => supportedBy(Row, contextText))

    if (supported.length === 0 || supported.length === rows.length)
    {
        return rows
    }

    return supported.concat(rows.filter(Row => supported.indexOf(Row) < 0))
}
