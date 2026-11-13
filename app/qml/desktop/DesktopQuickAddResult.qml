import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../MeaningPicks.js" as MeaningPicks

ColumnLayout
{
    id: root

    required property DictionaryController lookup
    required property var store
    property string term: ""
    property bool reverse: false
    property alias downloadTone: download.tone

    property bool searching: false
    property string pickedGist: ""
    property string meaningTranslation: ""
    property string openGist: ""

    signal wordClicked(string word)

    readonly property var entry: !root.reverse && root.term.length > 0 && !!root.lookup.entry.headword ? root.lookup.entry : null
    readonly property var meanings: root.entry ? (root.entry.meanings || []) : []
    readonly property bool hasMeanings: root.meanings.length > 0
    readonly property var pickedTranslations: MeaningPicks.parts(root.meaningTranslation)

    readonly property int defaultOpenIndex: Math.max(0, root.meanings.findIndex(Meaning => Meaning.learnedStage < 0 && Meaning.translations.length > 0))
    readonly property int pickedOpenIndex: root.meanings.findIndex(Meaning => Meaning.gist === root.openGist)
    readonly property int openIndex: root.pickedOpenIndex >= 0 ? root.pickedOpenIndex : (root.hasMeanings ? root.defaultOpenIndex : -1)

    readonly property bool translationShown: !root.reverse && root.term.length > 0 && !root.searching
                                             && root.lookup.results.length === 0
    readonly property bool translated: root.translationShown && !root.lookup.translating
                                       && root.lookup.machineTranslation.length > 0
    readonly property bool downloadShown: root.translationShown && !root.lookup.translating && !root.translated
                                          && AppMachineTranslation.offered
    readonly property bool phrase: root.wordCount(root.term) <= AppMetrics.readerLookupWordLimit
    readonly property bool addShown: root.entry !== null || (root.translated && root.phrase)
    readonly property bool canAdd: root.entry !== null ? root.meaningTranslation.length > 0 : root.translated && root.phrase

    function wordCount(text)
    {
        return text.split(/\s+/).filter(Part => Part.length > 0).length
    }

    function search()
    {
        root.resetPicks()

        if (root.term.length === 0)
        {
            root.searching = false
            root.lookup.clear()
            root.lookup.openEntry("", "")

            return
        }

        root.searching = true
        root.lookup.search(root.term)
    }

    function resetPicks()
    {
        root.pickedGist = ""
        root.meaningTranslation = ""
        root.openGist = ""
    }

    function pickDefault()
    {
        root.resetPicks()

        if (!root.entry)
        {
            return
        }

        if (root.hasMeanings)
        {
            const first = root.meanings.findIndex(Meaning => Meaning.learnedStage < 0 && Meaning.translations.length > 0)

            if (first >= 0)
            {
                root.pickTranslation(root.meanings[first].gist, root.meanings[first].translations[0])
            }

            return
        }

        const free = root.lookup.allTranslations.find(Translation => Translation.learnedStage < 0)

        if (free)
        {
            root.pickTranslation("", free.text)
        }
    }

    function pickTranslation(gist, text)
    {
        const picks = MeaningPicks.toggle(root.pickedGist, root.meaningTranslation, gist, text)

        root.pickedGist = picks.gist
        root.meaningTranslation = picks.text
    }

    function translationRow(text)
    {
        const wanted = text.trim().toLowerCase()

        return root.lookup.allTranslations.find(Translation => Translation.text.trim().toLowerCase() === wanted)
            || ({ text: text, relative: 0, hasProbability: false, learnedStage: -1 })
    }

    function add(vocabulary, hint)
    {
        if (!root.canAdd)
        {
            return false
        }

        if (root.entry === null)
        {
            const added = vocabulary.addCard(root.term, root.lookup.machineTranslation,
                                             root.wordCount(root.term) > 1 ? "phrase" : "", -1, false, "", hint)

            console.info("Quick Add", added ? "added" : "could not add", "a machine-translated phrase")

            return added
        }

        const meaning = root.meanings.find(Meaning => Meaning.gist === root.pickedGist)
        const gender = meaning ? meaning.gender : root.entry.headwordGender
        const headword = root.entry.headword
        const partOfSpeech = root.entry.partOfSpeech

        if (!vocabulary.addCard(headword, root.meaningTranslation, partOfSpeech, gender, gender >= 0, root.pickedGist, hint))
        {
            return false
        }

        console.info("Quick Add added a meaning of", headword)
        root.lookup.openEntry(headword, partOfSpeech, root.term)

        return true
    }

    spacing: AppMetrics.gapCompact

    Connections
    {
        target: root.lookup

        function onResultsChanged()
        {
            if (!root.searching)
            {
                return
            }

            root.searching = false

            if (root.reverse)
            {
                return
            }

            const row = root.lookup.results.find(Row => Row.exact) || root.lookup.results[0]

            if (!row)
            {
                console.info("Quick Add found nothing in the dictionary, translating")
            }

            root.lookup.openEntry(row ? row.word : "", row ? row.partOfSpeech : "", root.term)
        }

        function onEntryChanged() { Qt.callLater(root.pickDefault) }
    }

    DesktopLabel
    {
        objectName: "quick_add_translation"

        Layout.fillWidth: true
        Layout.leftMargin: AppMetrics.gapCompact
        Layout.rightMargin: AppMetrics.gapCompact
        visible: root.translationShown && !root.downloadShown
        text: root.lookup.translating ? qsTr("Translating")
            : root.translated ? root.lookup.machineTranslation
            : qsTr("No translation")
        color: root.translated ? AppTheme.colourText : AppTheme.colourMuted
        font.pixelSize: AppMetrics.fontLead
        wrapMode: Text.WordWrap
        textFormat: Text.PlainText
    }

    DesktopTranslationDownload
    {
        id: download

        Layout.fillWidth: true
        visible: root.downloadShown
        buttonName: "quick_add_download"
    }

    DesktopLabel
    {
        Layout.fillWidth: true
        Layout.leftMargin: AppMetrics.gapCompact
        Layout.rightMargin: AppMetrics.gapCompact
        visible: root.translated && AppMachineTranslation.attribution.length > 0
        text: AppMachineTranslation.attribution
        font.pixelSize: AppMetrics.fontSmall
        color: AppTheme.colourSubtle
        elide: Text.ElideRight
    }

    DesktopSelect
    {
        objectName: "quick_add_candidates"

        Layout.fillWidth: true
        visible: root.entry !== null && root.lookup.results.length > 1
        heading: qsTr("Dictionary word")
        options: root.lookup.results.map(Row => Row.displayWord || Row.word)
        descriptions: root.lookup.results.map(Row => Row.translation || "")
        currentIndex: root.entry ? root.lookup.results.findIndex(Row => Row.word === root.entry.headword) : -1

        onChosen: function(index)
        {
            const row = root.lookup.results[index]

            console.info("Quick Add opened another dictionary candidate")
            root.lookup.openEntry(row.word, row.partOfSpeech, root.term)
        }
    }

    Item
    {
        Layout.fillWidth: true
        Layout.leftMargin: AppMetrics.gapCompact
        Layout.rightMargin: AppMetrics.gapCompact
        implicitHeight: tabRow.implicitHeight
        visible: root.entry !== null && root.lookup.readings.length > 1

        Accessible.role: Accessible.PageTabList
        Accessible.name: qsTr("Parts of speech")

        DesktopDivider
        {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
        }

        Row
        {
            id: tabRow

            spacing: AppMetrics.gapLarge

            Repeater
            {
                model: root.entry !== null ? root.lookup.readings : []

                delegate: DesktopTab
                {
                    required property var modelData

                    text: modelData.label
                    selected: root.entry !== null && modelData.partOfSpeech === root.entry.partOfSpeech
                    stage: modelData.stage
                    completed: modelData.completed

                    onClicked: root.lookup.openEntry(root.entry.headword, modelData.partOfSpeech, root.term)
                }
            }
        }
    }

    Repeater
    {
        model: root.meanings

        delegate: DesktopTriageMeaning
        {
            required property var modelData
            required property int index

            objectName: "quick_add_meaning_" + index

            Layout.fillWidth: true
            meaning: modelData
            stageCount: root.store.stageCount
            chipColumns: 2
            translations: modelData.translations.map(Text => root.translationRow(Text))
            picked: root.pickedGist === modelData.gist ? root.pickedTranslations : []
            expanded: index === root.openIndex

            onExpandRequested: root.openGist = modelData.gist
            onTranslationPicked: function(text) { root.pickTranslation(modelData.gist, text) }
            onWordClicked: function(word) { root.wordClicked(word) }
        }
    }

    Grid
    {
        id: optionGrid

        readonly property real cellWidth: (width - columnSpacing * (columns - 1)) / columns

        Layout.fillWidth: true
        visible: root.entry !== null && !root.hasMeanings
        columns: 2
        columnSpacing: AppMetrics.gapSmall
        rowSpacing: AppMetrics.gapSmall

        Repeater
        {
            model: root.entry !== null && !root.hasMeanings ? root.lookup.allTranslations : []

            delegate: DesktopTranslationChip
            {
                required property var modelData

                width: optionGrid.cellWidth
                implicitHeight: AppMetrics.buttonHeight
                text: modelData.text
                relative: modelData.relative
                hasProbability: modelData.hasProbability
                learnedStage: modelData.learnedStage
                inUse: root.pickedTranslations.indexOf(modelData.text.trim().toLowerCase()) >= 0
                accentInUse: true
                enabled: modelData.learnedStage < 0
                focusPolicy: Qt.NoFocus

                onClicked: root.pickTranslation("", modelData.text)
            }
        }
    }
}
