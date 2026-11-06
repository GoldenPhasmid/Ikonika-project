.pragma library

function categories(decks)
{
    const names = []

    for (const deck of decks)
    {
        if (deck.category.length > 0 && names.indexOf(deck.category) < 0)
        {
            names.push(deck.category)
        }
    }

    return names
}

function tint(names, category, colours)
{
    const index = Math.max(0, names.indexOf(category))

    return colours[index % colours.length]
}

function summary(decks, category, locale)
{
    let words = 0
    let learned = 0

    for (const deck of decks)
    {
        if (deck.category === category)
        {
            words += deck.wordCount
            learned += deck.learnedCount
        }
    }

    const total = Number(words).toLocaleString(locale, "f", 0)

    return learned > 0 ? qsTr("%1 words · %2 learned").arg(total).arg(Number(learned).toLocaleString(locale, "f", 0))
                       : qsTr("%1 words").arg(total)
}

function detail(deck)
{
    if (deck.wordCount > 0 && deck.learnedCount >= deck.wordCount)
    {
        return qsTr("%1 words · all learned").arg(deck.wordCount)
    }

    return deck.learnedCount > 0 ? qsTr("%1 words · %2 learned").arg(deck.wordCount).arg(deck.learnedCount)
                                 : qsTr("%1 words").arg(deck.wordCount)
}
