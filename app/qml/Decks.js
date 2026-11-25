.pragma library

function customDecksShown()
{
    return false
}

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

function customTint(categories, categoryId, colours, fallback)
{
    const category = categories.find(entry => entry.id === categoryId)

    return category ? colours[category.colour % colours.length] : fallback
}

function importMessage(result)
{
    const message = qsTr("Imported %1: %2 new words, %3 already in My Vocabulary")
        .arg(result.name).arg(result.added).arg(result.reused)

    return result.moved > 0 ? message + qsTr(", %1 moved from another deck").arg(result.moved) : message
}

function summarise(decks, locale)
{
    let words = 0
    let learned = 0

    for (const deck of decks)
    {
        words += deck.wordCount
        learned += deck.learnedCount
    }

    const total = Number(words).toLocaleString(locale, "f", 0)

    return learned > 0 ? qsTr("%1 words · %2 learned").arg(total).arg(Number(learned).toLocaleString(locale, "f", 0))
                       : qsTr("%1 words").arg(total)
}

function summary(decks, category, locale)
{
    return summarise(decks.filter(deck => deck.category === category), locale)
}

function plain(text)
{
    return text.normalize("NFD").replace(/[̀-ͯ]/g, "").toLowerCase()
}

function matches(deck, term)
{
    return term.length === 0 || plain(deck.name).indexOf(plain(term)) >= 0
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

const sortOrders = ["created", "name"]

function sortLabels()
{
    return [qsTr("Recently created"), qsTr("Name")]
}

function sortDirections()
{
    return {
        "created": [qsTr("Newest first"), qsTr("Oldest first")],
        "name": [qsTr("A to Z"), qsTr("Z to A")]
    }
}

function sorted(decks, order, reversed)
{
    const byName = (first, second) => first.name.localeCompare(second.name)
    const compare = order === "name" ? byName
                                     : (first, second) => second.createdDate - first.createdDate || byName(first, second)

    return decks.slice().sort((first, second) => reversed ? compare(second, first) : compare(first, second))
}
