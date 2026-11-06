.pragma library

function fold(text)
{
    return text.normalize("NFD").replace(/[\u0300-\u036f]/g, "").toLowerCase()
}

const sortOrders = ["recent", "added", "title", "progress"]
const levels = ["", "beginner", "intermediate", "advanced"]

function levelLabels()
{
    return [qsTr("All levels"), qsTr("Beginner"), qsTr("Intermediate"), qsTr("Advanced")]
}

function levelLabel(level)
{
    const index = levels.indexOf(level)

    return index > 0 ? levelLabels()[index] : ""
}

function readingStage(document)
{
    if (document.progress < 0)
    {
        return 1
    }

    return document.progress >= 1 ? 2 : 0
}

function arrange(documents, sortOrder, term, reversed, level)
{
    const needle = fold(term)
    const shelf = documents.filter(function(document)
    {
        return (!level || document.level === level) && fold(document.title).indexOf(needle) >= 0
    })

    if (sortOrder === "title")
    {
        shelf.sort(function(left, right) { return left.title.localeCompare(right.title) })
    }
    else if (sortOrder === "added")
    {
        shelf.sort(function(left, right) { return right.added - left.added })
    }
    else if (sortOrder === "progress")
    {
        shelf.sort(function(left, right)
        {
            return readingStage(left) - readingStage(right)
                || right.progress - left.progress
                || right.lastOpened - left.lastOpened
        })
    }

    if (reversed)
    {
        shelf.reverse()
    }

    return shelf
}

function tint(title, colours)
{
    let hash = 0

    for (let index = 0; index < title.length; ++index)
    {
        hash = (hash * 31 + title.charCodeAt(index)) % 1000003
    }

    return colours[hash % colours.length]
}

function brief(book)
{
    const percent = Math.round(Math.max(0, book.progress) * 100)

    if (book.progress >= 0)
    {
        return book.local ? qsTr("Uploading %1%").arg(percent) : qsTr("Downloading %1%").arg(percent)
    }

    if (!book.local)
    {
        return qsTr("%1 MB").arg((book.size / 1048576).toFixed(1))
    }

    return book.readProgress >= 0 ? qsTr("%1%").arg(Math.round(book.readProgress * 100)) : ""
}

function place(book, localIcon)
{
    if (!book.local)
    {
        return "cloud_download"
    }

    return book.inCloud ? "cloud_check" : localIcon
}

function placeText(book, localText)
{
    if (!book.local)
    {
        return book.free ? qsTr("Free to download") : qsTr("In the cloud")
    }

    return book.inCloud ? qsTr("Here and in the cloud") : localText
}
