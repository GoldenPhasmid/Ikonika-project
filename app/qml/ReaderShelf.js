.pragma library

function fold(text)
{
    return text.normalize("NFD").replace(/[\u0300-\u036f]/g, "").toLowerCase()
}

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

function tint(title, colours)
{
    let hash = 0

    for (let index = 0; index < title.length; ++index)
    {
        hash = (hash * 31 + title.charCodeAt(index)) % 1000003
    }

    return colours[hash % colours.length]
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

function libraryOrderDirections()
{
    return {
        "recent": [qsTr("Newest first"), qsTr("Oldest first")],
        "title": [qsTr("A to Z"), qsTr("Z to A")],
        "author": [qsTr("A to Z"), qsTr("Z to A")],
        "level": [qsTr("Easiest first"), qsTr("Hardest first")],
        "progress": [qsTr("In progress first"), qsTr("Finished first")],
        "added": [qsTr("Newest first"), qsTr("Oldest first")],
        "size": [qsTr("Largest first"), qsTr("Smallest first")]
    }
}

function emptyFilter()
{
    return { "levels": [], "statuses": [], "storage": [], "formats": [] }
}

function toggledFilter(filter, key, value)
{
    const next = { "levels": filter.levels.slice(), "statuses": filter.statuses.slice(),
                   "storage": filter.storage.slice(), "formats": filter.formats.slice() }
    const index = next[key].indexOf(value)

    if (index >= 0)
    {
        next[key].splice(index, 1)
    }
    else
    {
        next[key].push(value)
    }

    return next
}

function formats(books)
{
    return books.map(function(book) { return (book.format || "").toLowerCase() })
                .filter(function(format, index, all) { return format.length > 0 && all.indexOf(format) === index })
                .sort()
}

function sectionCounts(books, freeBooks, collections)
{
    const counts = { "free": freeBooks.length }
    const keys = ["all", "reading", "finished", "favorites"]

    for (const collection of collections)
    {
        keys.push("collection:" + collection.id)
    }

    for (const key of keys)
    {
        counts[key] = books.filter(function(book) { return inSection(book, key) }).length
    }

    return counts
}

function sectionTitle(section, collections)
{
    const titles = {
        "all": qsTr("My books"),
        "reading": qsTr("Reading"),
        "finished": qsTr("Finished"),
        "favorites": qsTr("Favorites"),
        "free": qsTr("Free library")
    }

    if (titles[section] !== undefined)
    {
        return titles[section]
    }

    const collection = collections.find(function(entry) { return "collection:" + entry.id === section })

    return collection ? collection.name : titles["all"]
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

    const ordered = compare(order)
    const positions = new Map(shelf.map(function(book, index) { return [book, index] }))

    shelf.sort(function(left, right) { return ordered(left, right) || positions.get(left) - positions.get(right) })

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

const readingLevelWords = [1500, 4000]

function readingLevel(knownWords)
{
    if (knownWords < readingLevelWords[0])
    {
        return "beginner"
    }

    return knownWords < readingLevelWords[1] ? "intermediate" : "advanced"
}

function startBooks(freeBooks, knownWords)
{
    const target = levels.indexOf(readingLevel(knownWords))

    function distance(book)
    {
        const index = levels.indexOf(book.level)

        return index > 0 ? Math.abs(index - target) : levels.length
    }

    return freeBooks.filter(function(book) { return status(book) === "new" })
                    .sort(function(left, right)
                    {
                        return distance(left) - distance(right)
                            || Number(right.local) - Number(left.local)
                            || left.title.localeCompare(right.title)
                    })
}

function continueBooks(books)
{
    return books.filter(function(book) { return status(book) === "reading" && book.exists })
                .sort(compare("recent"))
}
