.pragma library

function startPlace(list, firstIndex, top)
{
    for (let index = firstIndex; index < list.count; ++index)
    {
        const row = list.itemAtIndex(index)

        if (row === null)
        {
            break
        }

        if (row.pictured || row.rule)
        {
            continue
        }

        return { paragraph: index, character: row.characterAt(top - row.y) }
    }

    return { paragraph: firstIndex, character: 0 }
}

function sentenceSpan(list, readAloud)
{
    const row = list.itemAtIndex(readAloud.paragraph)

    if (row === null)
    {
        return null
    }

    const bounds = row.sentenceBounds(readAloud.sentenceStart, readAloud.sentenceEnd)

    return { top: row.y + bounds.top, bottom: row.y + bounds.bottom }
}

function isShown(list, paragraph, start, end, visibleTop, visibleBottom)
{
    if (paragraph < 0)
    {
        return false
    }

    const row = list.itemAtIndex(paragraph)

    if (row === null)
    {
        return false
    }

    const bounds = row.sentenceBounds(start, end)

    return row.y + bounds.bottom > visibleTop && row.y + bounds.top < visibleBottom
}

function isReadingShown(list, readAloud, visibleTop, visibleBottom)
{
    return isShown(list, readAloud.paragraph, readAloud.sentenceStart, readAloud.sentenceEnd, visibleTop, visibleBottom)
}

function play(list, readAloud, firstIndex, visibleTop, visibleBottom)
{
    if (readAloud.started && isReadingShown(list, readAloud, visibleTop, visibleBottom))
    {
        readAloud.resume()

        return
    }

    if (!readAloud.started && isShown(list, readAloud.savedParagraph, readAloud.savedCharacter,
                                      readAloud.savedCharacter + 1, visibleTop, visibleBottom))
    {
        readAloud.startAt(readAloud.savedParagraph, readAloud.savedCharacter)

        return
    }

    const place = startPlace(list, firstIndex, visibleTop)

    readAloud.startAt(place.paragraph, place.character)
}
