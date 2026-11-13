.pragma library

function parts(text)
{
    return text.split(",")
        .map(Part => Part.trim().toLowerCase())
        .filter(Part => Part.length > 0)
}

function toggle(pickedGist, pickedText, gist, text)
{
    if (pickedGist !== gist)
    {
        return { gist: gist, text: text }
    }

    const wanted = text.trim().toLowerCase()
    const kept = pickedText.split(",")
        .map(Part => Part.trim())
        .filter(Part => Part.length > 0 && Part.toLowerCase() !== wanted)

    if (kept.length === parts(pickedText).length)
    {
        kept.push(text)
    }

    return { gist: kept.length > 0 ? gist : "", text: kept.join(", ") }
}
