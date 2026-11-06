import QtQuick

QtObject
{
    id: root

    property var vocabulary: null
    property var filterKeys: root.allFilterKeys
    property bool reverse: false
    property int wordCount: 0
    property int cardCount: 0
    property int repetitionCount: 0

    readonly property var allFilterKeys: ["notlearned", "learning", "completed"]
    readonly property int minimumCards: 1
    readonly property int defaultCards: 20
    readonly property int minimumRepetitions: 5
    readonly property int maximumRepetitions: 100
    readonly property int repetitionStep: 5

    readonly property int maximumCards: Math.max(root.wordCount, root.minimumCards)
    readonly property bool canChooseCards: root.wordCount > root.minimumCards

    readonly property var filters: [
        { key: "notlearned", label: qsTr("New"), count: root.vocabulary ? root.vocabulary.notLearnedCount : 0 },
        { key: "learning", label: qsTr("Reviewing"), count: root.vocabulary ? root.vocabulary.learningCount : 0 },
        { key: "completed", label: qsTr("Mastered"), count: root.vocabulary ? root.vocabulary.learnedCount : 0 }
    ]

    function isSelected(key)
    {
        return root.filterKeys.indexOf(key) >= 0
    }

    function isAvailable(filter)
    {
        return filter.count > 0 || root.isSelected(filter.key)
    }

    function update()
    {
        root.wordCount = root.vocabulary ? root.vocabulary.practiceCount(root.filterKeys) : 0

        if (root.wordCount === 0)
        {
            root.cardCount = 0

            return
        }

        root.cardCount = root.cardCount === 0 ? Math.min(root.wordCount, root.defaultCards)
                                              : Math.min(root.cardCount, root.wordCount)
    }

    function reset()
    {
        if (root.vocabulary)
        {
            root.vocabulary.refresh()
        }

        const populatedKeys = root.filters.filter(function(filter) { return filter.count > 0 })
                                          .map(function(filter) { return filter.key })

        root.filterKeys = populatedKeys.length > 0 ? populatedKeys : root.allFilterKeys
        root.reverse = false
        root.cardCount = 0
        root.update()

        const wholePasses = Math.ceil(root.cardCount / root.repetitionStep) * root.repetitionStep

        root.repetitionCount = Math.max(root.minimumRepetitions, Math.min(root.maximumRepetitions, wholePasses))
    }

    function toggleFilter(key)
    {
        const keys = root.isSelected(key) ? root.filterKeys.filter(function(other) { return other !== key })
                                          : root.filterKeys.concat([key])

        if (keys.length === 0)
        {
            return
        }

        root.filterKeys = keys
        root.update()
    }

    property Connections vocabularyWatcher: Connections
    {
        target: root.vocabulary

        function onVocabularyChanged() { root.update() }
    }
}
