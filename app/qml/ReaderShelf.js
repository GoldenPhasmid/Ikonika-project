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

const libraryOrders = ["recent", "title", "author", "level", "progress", "added", "size"]
const libraryGroups = ["none", "level", "author"]

function libraryOrderLabels()
{
    return [qsTr("Recently opened"), qsTr("Title"), qsTr("Author"), qsTr("Level"), qsTr("Progress"),
            qsTr("Date added"), qsTr("File size")]
}

function libraryGroupLabels()
{
    return [qsTr("None"), qsTr("Level"), qsTr("Author")]
}

function status(book)
{
    if (book.finished)
    {
        return "finished"
    }

    return book.progress > 0 && book.local ? "reading" : "new"
}

function inSection(book, section)
{
    if (section === "reading")
    {
        return status(book) === "reading"
    }

    if (section === "finished")
    {
        return book.finished
    }

    if (section === "favorites")
    {
        return book.favorite
    }

    if (section.indexOf("collection:") === 0)
    {
        return (book.collections || []).indexOf(section.substring("collection:".length)) >= 0
    }

    return true
}

function storage(book)
{
    return book.local ? "downloaded" : "cloud"
}

function passes(book, filter)
{
    const checks = [
        [filter.levels, book.level || ""],
        [filter.statuses, status(book)],
        [filter.storage, storage(book)],
        [filter.formats, (book.format || "").toLowerCase()]
    ]

    return checks.every(function(check) { return check[0].length === 0 || check[0].indexOf(check[1]) >= 0 })
}

function filterCount(filter)
{
    return filter.levels.length + filter.statuses.length + filter.storage.length + filter.formats.length
}

function levelRank(book)
{
    const index = levels.indexOf(book.level || "")

    return index > 0 ? index : levels.length
}

function compare(order)
{
    if (order === "title")
    {
        return function(left, right) { return left.title.localeCompare(right.title) }
    }

    if (order === "author")
    {
        return function(left, right)
        {
            return (!left.author) - (!right.author)
                || (left.author || "").localeCompare(right.author || "")
                || left.title.localeCompare(right.title)
        }
    }

    if (order === "level")
    {
        return function(left, right) { return levelRank(left) - levelRank(right) || left.title.localeCompare(right.title) }
    }

    if (order === "progress")
    {
        return function(left, right)
        {
            return readingStage(left) - readingStage(right)
                || right.progress - left.progress
                || right.lastOpened - left.lastOpened
        }
    }

    if (order === "added")
    {
        return function(left, right) { return (right.added || 0) - (left.added || 0) }
    }

    if (order === "size")
    {
        return function(left, right) { return (right.size || 0) - (left.size || 0) }
    }

    return function(left, right) { return (right.lastOpened || 0) - (left.lastOpened || 0) }
}

function shelve(books, section, filter, term, order, reversed)
{
    const needle = fold(term)
    const shelf = books.filter(function(book)
    {
        return inSection(book, section) && passes(book, filter)
            && (fold(book.title).indexOf(needle) >= 0 || fold(book.author || "").indexOf(needle) >= 0)
    })

    shelf.sort(compare(order))

    if (reversed)
    {
        shelf.reverse()
    }

    return shelf
}

function group(books, mode)
{
    if (mode !== "level" && mode !== "author")
    {
        return [{ "title": "", "books": books }]
    }

    const groups = []
    const byName = {}

    for (const book of books)
    {
        const name = mode === "level" ? (levelLabel(book.level || "") || qsTr("No level"))
                                      : (book.author || qsTr("Unknown author"))

        if (byName[name] === undefined)
        {
            byName[name] = groups.length
            groups.push({ "title": name, "books": [] })
        }

        groups[byName[name]].books.push(book)
    }

    if (mode === "level")
    {
        groups.sort(function(left, right) { return levelRank(left.books[0]) - levelRank(right.books[0]) })
    }
    else
    {
        groups.sort(function(left, right)
        {
            return (!left.books[0].author) - (!right.books[0].author) || left.title.localeCompare(right.title)
        })
    }

    return groups
}

function rows(groups, columns)
{
    const result = []
    let first = 0

    for (const group of groups)
    {
        if (group.title.length > 0)
        {
            result.push({ "title": group.title })
        }

        for (let start = 0; start < group.books.length; start += columns)
        {
            result.push({ "books": group.books.slice(start, start + columns), "first": first + start })
        }

        first += group.books.length
    }

    return result
}

function continueBooks(books)
{
    return books.filter(function(book) { return status(book) === "reading" && book.exists })
                .sort(compare("recent"))
}
