import QtQuick
import QtQuick.Layouts

MobileBottomSheet
{
    id: root

    property bool shown: false
    property bool pending: false
    property bool focusWanted: false

    readonly property string term: (lookupField.text + lookupField.preeditText).trim()
    readonly property alias dictionary: lookup

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
        lookup.search(word)
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

    heading: qsTr("Dictionary")
    maxHeightFraction: 0.85
    bottomInset: root.SafeArea.margins.bottom
    enabled: !grace.running

    onDismissed: root.close()

    DictionaryController
    {
        id: lookup

        onResultsChanged: if (root.pending) root.reveal()
    }

    Timer
    {
        id: grace

        interval: Qt.styleHints.mouseDoubleClickInterval
    }

    MobileSearchField
    {
        id: lookupField

        onTextChanged: lookup.search((lookupField.text + lookupField.preeditText).trim())
        onPreeditTextChanged: lookup.search((lookupField.text + lookupField.preeditText).trim())
    }

    Item
    {
        readonly property real matchListHeight: 56 * 2 + AppMetrics.gapCompact
        readonly property real resultsHeight: lookup.results.length > 0
            ? matchListHeight : Math.max(matchListHeight, emptyResults.implicitHeight)

        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(resultsHeight,
            Math.max(0, root.maximumBodyHeight - lookupField.Layout.preferredHeight - AppMetrics.gapWide))

        MobileWordCardList
        {
            anchors.fill: parent
            visible: lookup.results.length > 0

            model: lookup.results
            maxStage: 1

            showAddAction: true
            showEditAction: false
            showDeleteAction: false

            onWordClicked: function(index)
            {
                const row = lookup.results[index]
                root.close()
                root.entryOpened(row.word, "", root.sourceSentence)
            }

            onVoicePlayRequested: function(index)
            {
                lookup.speak(lookup.results[index].word)
            }

            onAddRequested: function(index)
            {
                const row = lookup.results[index]
                root.close()
                root.addWordRequested(row.word, row.translation.split(", ")[0] || "", row.partOfSpeech, root.sourceSentence)
            }
        }

        Flickable
        {
            anchors.fill: parent
            visible: lookup.results.length === 0
            contentWidth: width
            contentHeight: emptyResults.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ColumnLayout
            {
                id: emptyResults

                width: parent.width
                spacing: AppMetrics.gapWide

                MobileMachineTranslation
                {
                    Layout.fillWidth: true
                    visible: AppMachineTranslation.enabled && lookup.results.length === 0 && root.term.length > 0
                    lookup: root.dictionary
                    term: root.term

                    onAddRequested: function(word, translation)
                    {
                        root.close()
                        root.addWordRequested(word, translation, "", root.sourceSentence)
                    }
                }

                Text
                {
                    Layout.fillWidth: true
                    visible: lookup.results.length === 0 && root.term.length > 0 && !lookup.translating && lookup.machineTranslation.length === 0

                    text: qsTr("Not in the dictionary — you can still add it by hand.")
                    color: AppTheme.colourMuted
                    font.pixelSize: AppMetrics.fontBody
                    font.family: AppMetrics.fontFamily
                    wrapMode: Text.WordWrap
                }

                MobileWideButton
                {
                    Layout.fillWidth: true
                    visible: lookup.results.length === 0 && root.term.length > 0 && !lookup.translating && lookup.machineTranslation.length === 0

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
