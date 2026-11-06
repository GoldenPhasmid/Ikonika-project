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
