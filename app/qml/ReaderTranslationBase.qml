import QtQuick
import "ReaderTranslationMatches.js" as Matches

Item
{
    id: root

    required property DictionaryController contextLookup
    required property var lookup
    property var geometry: null
    property string text: ""
    property string sentence: ""
    property int maxStage: 1
    property bool readAloudOffered: false
    property bool knownOffered: false

    property bool open: false
    property bool searching: false

    Binding
    {
        target: root.lookup
        property: "separatePartsOfSpeech"
        value: true
    }

    signal addWordRequested(string word, string translation, string partOfSpeech, string hint)
    signal dictionaryRequested(string word)
    signal readAloudRequested()
    signal knownMarkChanged()
    signal shown()

    readonly property int words: Matches.wordCount(root.text)
    readonly property bool brief: root.words > 0 && root.words <= AppMetrics.readerLookupWordLimit
    readonly property var matches: root.open && root.brief && !root.searching ? Matches.findMatches(root.lookup.results) : []
    readonly property var shownMatches: Matches.rankMatches(root.matches, root.contextLookup.machineTranslation)
    readonly property var match: root.shownMatches.length > 0 ? root.shownMatches[0] : null
    readonly property bool cardShown: root.match !== null
    readonly property bool knowable: root.knownOffered && root.match !== null && (root.match.stage ?? -1) < 0
                                     && !root.match.completed
    readonly property bool known: root.match !== null && !!root.match.known
    readonly property bool translated: !root.lookup.translating && root.lookup.machineTranslation.length > 0
    readonly property bool downloadShown: !root.cardShown && !root.lookup.translating && !root.translated
                                          && root.lookup.machinePack.offered
    readonly property bool actionsShown: !root.downloadShown && !root.lookup.translating
                                         && (root.cardShown || root.translated || root.brief)
    readonly property bool onScreen: root.geometry !== null && root.geometry.endY >= 0 && root.geometry.endY <= root.height

    function openFor(term, sourceSentence)
    {
        root.sentence = sourceSentence || ""
        root.text = term
        root.show()
    }

    function show()
    {
        root.shown()
        console.info("Learner asked to translate", root.words, "words selected in the book")

        root.open = true

        if (!root.brief)
        {
            root.lookup.translate(root.text)

            return
        }

        root.searching = true
        root.lookup.search(root.text)
    }

    function refreshMatches()
    {
        if (root.open && root.cardShown)
        {
            root.lookup.search(root.text)
        }
    }

    function dismiss()
    {
        if (!root.open)
        {
            return false
        }

        root.open = false

        return true
    }

    function addMatch(row)
    {
        root.addWordRequested(row.word, (row.translation || "").split(", ")[0], row.partOfSpeech || "", root.sentence)
    }

    function toggleKnown()
    {
        if (!root.knowable)
        {
            return
        }

        const changed = root.known ? root.lookup.unmarkKnown(root.match.word, root.match.partOfSpeech)
                                   : root.lookup.markKnown(root.match.word, root.match.partOfSpeech)

        if (changed)
        {
            root.knownMarkChanged()
        }
    }

    function addWord()
    {
        if (root.match !== null)
        {
            root.addMatch(root.match)

            return
        }

        const phrase = root.text.trim()

        root.addWordRequested(phrase, root.lookup.machineTranslation, root.words > 1 ? "phrase" : "",
                              root.sentence.trim() === phrase ? "" : root.sentence)
    }

    function translateAfterDownload()
    {
        if (!root.open || root.searching || root.cardShown || root.lookup.translating || root.translated)
        {
            return
        }

        console.info("Translating the book selection once the translation model arrived")

        root.lookup.translate(root.text)
    }

    function translateContext()
    {
        root.contextLookup.translate("")

        if (root.matches.length > 1 && root.sentence.trim().length > 0)
        {
            console.info("Translating the sentence to rank", root.matches.length, "dictionary matches")
            root.contextLookup.translate(root.sentence)
        }
    }

    onTextChanged:
    {
        root.open = false
        root.searching = false
        root.contextLookup.translate("")
    }
    onGeometryChanged: if (root.geometry === null) root.open = false

    Connections
    {
        target: root.lookup.machinePack

        function onChanged()
        {
            if (root.lookup.machinePack.installed)
            {
                root.translateAfterDownload()
            }
        }
    }

    Connections
    {
        target: root.lookup
        enabled: root.searching

        function onResultsChanged()
        {
            root.searching = false

            if (root.lookup.results.length > 0 && Matches.findMatches(root.lookup.results).length === 0)
            {
                root.lookup.translate(root.text)

                return
            }

            root.translateContext()
        }
    }
}
