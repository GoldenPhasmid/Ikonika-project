import QtQuick
import QtQuick.Layouts

MobileBottomSheet
{
    id: root

    required property DictionaryController lookup
    required property int stageCount

    property bool shown: false
    property bool pending: false
    property bool focusWanted: false
    property bool readerMode: false

    readonly property string term: (lookupField.text + lookupField.preeditText).trim()
    readonly property DictionaryController dictionary: root.lookup
    readonly property bool hasFormTranslation: AppMachineTranslation.enabled
        && root.dictionary.results.length > 0 && root.dictionary.formTerm.length > 0

    property string sourceSentence: ""

    signal addWordRequested(string word, string translation, string partOfSpeech, string hint)
    signal entryOpened(string headword, string partOfSpeech, string hint)

    function openFor(word, focusField, sentence)
    {
        root.sourceSentence = sentence || ""
        root.focusWanted = focusField
        root.pending = true

        lookupField.text = word
        if (!focusField)
        {
            lookupField.cursorPosition = 0
        }
        root.lookup.search(word)
    }

    function close()
    {
        root.pending = false
        lookupField.focus = false
        root.shown = false
    }

    function reveal()
    {
        root.pending = false
        root.shown = true
        grace.restart()

        if (root.focusWanted)
        {
            lookupField.forceActiveFocus()
        }
    }

    z: 50
    visible: root.shown

    heading: root.readerMode ? "" : qsTr("Dictionary")
    showClose: !root.readerMode
    bottomInset: root.SafeArea.margins.bottom
    enabled: !grace.running

    onDismissed: root.close()

    Connections
    {
        target: root.lookup

        function onResultsChanged() { if (root.pending) root.reveal() }
    }

    Timer
    {
        id: grace

        interval: Qt.styleHints.mouseDoubleClickInterval
    }

    Component
    {
        id: machineTranslationResult

        MobileMachineTranslation
        {
            lookup: root.dictionary
            term: root.term

            onAddRequested: function(word, translation)
            {
                root.close()
                root.addWordRequested(word, translation, "", root.sourceSentence)
            }
        }
    }

    Component
    {
        id: machineTranslationHeader

        Item
        {
            width: ListView.view.width
            height: translationLoader.height + AppMetrics.gapCompact

            Loader
            {
                id: translationLoader

                width: parent.width
                sourceComponent: machineTranslationResult
            }
        }
    }

    MobileSearchField
    {
        id: lookupField

        onTextChanged: root.lookup.search((lookupField.text + lookupField.preeditText).trim())
        onPreeditTextChanged: root.lookup.search((lookupField.text + lookupField.preeditText).trim())
    }

    Item
    {
        readonly property real rowHeight: 56
        readonly property int matchCount: root.lookup.results.length
        readonly property real matchListHeight: root.readerMode
            ? Math.min(rowHeight * 3.2 + AppMetrics.gapCompact * 3,
                       rowHeight * matchCount + AppMetrics.gapCompact * Math.max(0, matchCount - 1) + matches.headerHeight)
            : rowHeight * 2 + AppMetrics.gapCompact
        readonly property real resultsHeight: matchCount > 0 ? matchListHeight
            : root.readerMode ? emptyResults.implicitHeight : Math.max(matchListHeight, emptyResults.implicitHeight)

        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(resultsHeight,
            Math.max(0, root.maximumBodyHeight - lookupField.Layout.preferredHeight - AppMetrics.gapWide))

        Loader
        {
            id: formTranslation

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top

            active: !root.readerMode && root.hasFormTranslation
            visible: active
            sourceComponent: machineTranslationResult
        }

        MobileWordCardList
        {
            id: matches

            anchors.fill: parent
            anchors.topMargin: formTranslation.visible ? formTranslation.height + AppMetrics.gapSmall : 0
            visible: root.lookup.results.length > 0

            model: root.lookup.results
            maxStage: root.stageCount
            header: root.readerMode && root.hasFormTranslation ? machineTranslationHeader : null

            showAddAction: true
            showEditAction: false
            showDeleteAction: false

            onWordClicked: function(index)
            {
                const row = root.lookup.results[index]
                root.close()
                root.entryOpened(row.word, "", root.sourceSentence)
            }

            onVoicePlayRequested: function(index)
            {
                root.lookup.speak(root.lookup.results[index].word)
            }

            onAddRequested: function(index)
            {
                const row = root.lookup.results[index]
                root.close()
                root.addWordRequested(row.word, row.translation.split(", ")[0] || "", row.partOfSpeech, root.sourceSentence)
            }
        }

        Flickable
        {
            anchors.fill: parent
            visible: root.lookup.results.length === 0
            contentWidth: width
            contentHeight: emptyResults.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ColumnLayout
            {
                id: emptyResults

                width: parent.width
                spacing: AppMetrics.gapWide

                Loader
                {
                    Layout.fillWidth: true
                    active: AppMachineTranslation.enabled && root.lookup.results.length === 0 && root.term.length > 0
                    visible: active
                    sourceComponent: machineTranslationResult
                }

                MobileWideButton
                {
                    Layout.fillWidth: true
                    visible: root.lookup.results.length === 0 && root.term.length > 0 && !root.lookup.translating && root.lookup.machineTranslation.length === 0

                    label: qsTr("Add as a word")

                    onClicked:
                    {
                        const typed = root.term
                        root.close()
                        root.addWordRequested(typed, "", "", root.sourceSentence)
                    }
                }
            }
        }
    }
}
