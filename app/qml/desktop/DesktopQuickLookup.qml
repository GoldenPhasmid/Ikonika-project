import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Window

Window
{
    id: root

    required property QtObject service
    required property VocabularyController vocabulary

    signal openEntryRequested(string word, string partOfSpeech)

    property bool searching: false
    property bool wasActive: false
    property string errorText: ""
    property var choices: []

    title: qsTr("Ikonika Quick Add")
    width: 456
    height: 340
    minimumWidth: width
    maximumWidth: width
    minimumHeight: height
    maximumHeight: height
    flags: Qt.Tool | Qt.WindowStaysOnTopHint | Qt.WindowTitleHint | Qt.WindowCloseButtonHint
    transientParent: null
    color: AppTheme.colourPanel
    palette.base: AppTheme.colourPanel
    palette.text: AppTheme.colourText
    palette.button: AppTheme.colourPanel
    palette.buttonText: AppTheme.colourText
    palette.highlight: AppTheme.colourAccent
    palette.highlightedText: AppTheme.colourAccentText

    function search()
    {
        root.choices = []
        root.errorText = ""
        root.searching = query.text.trim().length > 0

        if (root.searching)
        {
            lookup.search(query.text)
        }
        else
        {
            lookup.clear()
        }
    }

    function showSelection(text)
    {
        searchDelay.stop()
        query.text = text
        manual.text = ""
        root.search()
        focusGrace.restart()
        root.service.present(root)
        query.forceActiveFocus()
    }

    function closeLookup(restoreFocus)
    {
        root.wasActive = false
        focusGrace.stop()
        searchDelay.stop()
        root.service.dismiss(root, restoreFocus)
        lookup.clear()
        query.clear()
        manual.clear()
        root.choices = []
    }

    function openResult(index)
    {
        const result = lookup.results[index]
        if (!result)
        {
            return
        }

        lookup.openEntry(result.word, result.partOfSpeech, query.text)
        reading.currentIndex = lookup.readings.findIndex(item => item.partOfSpeech === lookup.entry.partOfSpeech)
    }

    function updateChoices()
    {
        const entry = lookup.entry
        const choices = []

        for (const meaning of entry.meanings || [])
        {
            for (const translation of meaning.translations)
            {
                choices.push({word: entry.headword, partOfSpeech: entry.partOfSpeech, translation: translation,
                              gist: meaning.gist, gender: meaning.gender, added: !!meaning.cardId})
            }
        }

        if (choices.length === 0)
        {
            for (const translation of lookup.allTranslations)
            {
                choices.push({word: entry.headword, partOfSpeech: entry.partOfSpeech, translation: translation.text,
                              gist: "", gender: entry.headwordGender, added: !!translation.cardId})
            }
        }

        root.choices = choices
        resultList.currentIndex = choices.findIndex(choice => !choice.added)
    }

    function save(choice)
    {
        if (!choice || choice.added || root.searching || searchDelay.running)
        {
            return
        }

        if (root.vocabulary.addCard(choice.word, choice.translation, choice.partOfSpeech,
                                    choice.gender, choice.gender >= 0, choice.gist, ""))
        {
            root.closeLookup(true)
            return
        }

        root.errorText = qsTr("Could not add this word")
    }

    function saveManual()
    {
        const word = query.text.trim()
        const translation = manual.text.trim()

        if (!word || !translation)
        {
            return
        }

        if (lookup.cards.some(card => card.word.toLowerCase() === word.toLowerCase()
                                      && card.translation.toLowerCase() === translation.toLowerCase()))
        {
            root.errorText = qsTr("Already added")
            return
        }

        root.save({word: word, translation: translation, partOfSpeech: "", gender: -1, gist: ""})
    }

    onClosing: function(event)
    {
        event.accepted = false
        root.closeLookup(true)
    }

    onActiveChanged:
    {
        if (focusGrace.running)
        {
            return
        }

        if (active)
        {
            wasActive = true
        }
        else if (wasActive && !wordPicker.popup.visible && !reading.popup.visible)
        {
            root.closeLookup(false)
        }
    }

    Shortcut
    {
        sequence: "Escape"
        onActivated: root.closeLookup(true)
    }

    Connections
    {
        target: root.service

        function onLookupRequested(text) { root.showSelection(text) }

        function onEnabledChanged()
        {
            if (root.visible && !root.service.enabled)
            {
                root.closeLookup(false)
            }
        }
    }

    DictionaryController
    {
        id: lookup

        onResultsChanged:
        {
            if (!root.searching || searchDelay.running)
            {
                return
            }

            root.searching = false
            const exact = results.findIndex(result => result.exact)
            wordPicker.currentIndex = exact >= 0 ? exact : results.length > 0 ? 0 : -1
            root.openResult(wordPicker.currentIndex)
        }

        onEntryChanged: root.updateChoices()
        onLanguagesChanged:
        {
            if (root.visible)
            {
                root.closeLookup(false)
            }
        }
    }

    Timer
    {
        id: focusGrace
        interval: 200
        onTriggered: root.wasActive = root.active
    }

    Timer
    {
        id: searchDelay
        interval: 140
        onTriggered: root.search()
    }

    ColumnLayout
    {
        anchors.fill: parent
        anchors.margins: AppMetrics.gapMedium
        spacing: AppMetrics.gapCompact

        RowLayout
        {
            TextField
            {
                id: query

                Layout.fillWidth: true
                Accessible.name: qsTr("Word to look up")
                maximumLength: 160
                selectByMouse: true
                onTextEdited: searchDelay.restart()
                Keys.onUpPressed: resultList.decrementCurrentIndex()
                Keys.onDownPressed: resultList.incrementCurrentIndex()
                onAccepted: root.save(root.choices[resultList.currentIndex])
            }

            DesktopLabel
            {
                text: lookup.targetLanguageCode.toUpperCase() + " → " + lookup.nativeLanguageCode.toUpperCase()
            }
        }

        RowLayout
        {
            visible: query.text.trim().length > 0 && !root.searching && lookup.results.length > 0 && !searchDelay.running

            ComboBox
            {
                id: wordPicker

                Layout.fillWidth: true
                Accessible.name: qsTr("Dictionary word")
                model: lookup.results
                textRole: "displayWord"
                onActivated: function(index) { root.openResult(index) }
            }

            ComboBox
            {
                id: reading

                visible: count > 1
                Accessible.name: qsTr("Part of speech")
                model: lookup.readings
                textRole: "label"
                onActivated: function(index)
                {
                    lookup.openEntry(lookup.entry.headword, lookup.readings[index].partOfSpeech, query.text)
                }
            }
        }

        ListView
        {
            id: resultList

            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.choices.length > 0 && !searchDelay.running
            clip: true
            model: root.choices
            ScrollBar.vertical: DesktopScrollBar {}

            delegate: Rectangle
            {
                id: row

                required property var modelData
                required property int index

                width: ListView.view.width - AppMetrics.gapCompact
                height: rowContent.implicitHeight + AppMetrics.gapCompact
                color: ListView.isCurrentItem ? AppTheme.colourBorder : AppTheme.colourPanel
                radius: AppMetrics.radiusMedium

                RowLayout
                {
                    id: rowContent

                    anchors.fill: parent
                    anchors.leftMargin: AppMetrics.gapCompact
                    anchors.rightMargin: AppMetrics.gapCompact

                    ColumnLayout
                    {
                        Layout.fillWidth: true
                        spacing: 0

                        DesktopLabel
                        {
                            Layout.fillWidth: true
                            text: row.modelData.translation
                            elide: Text.ElideRight
                        }

                        DesktopLabel
                        {
                            Layout.fillWidth: true
                            visible: text.length > 0
                            text: row.modelData.gist
                            color: AppTheme.colourMuted
                            elide: Text.ElideRight
                        }
                    }

                    DesktopButton
                    {
                        text: row.modelData.added ? qsTr("Already added") : qsTr("Add")
                        tone: row.ListView.isCurrentItem ? "primary" : "secondary"
                        enabled: !row.modelData.added
                        Accessible.name: row.modelData.added ? text : qsTr("Add %1").arg(row.modelData.translation)
                        onClicked: root.save(row.modelData)
                    }
                }
            }
        }

        ColumnLayout
        {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !resultList.visible

            DesktopLabel
            {
                visible: query.text.trim().length > 0
                text: root.searching || searchDelay.running ? qsTr("Looking up") : qsTr("No dictionary translation")
            }

            RowLayout
            {
                visible: query.text.trim().length > 0 && !root.searching && !searchDelay.running

                TextField
                {
                    id: manual

                    Layout.fillWidth: true
                    placeholderText: qsTr("Enter translation")
                    Accessible.name: placeholderText
                    onAccepted: root.saveManual()
                }

                DesktopButton
                {
                    text: qsTr("Add")
                    enabled: manual.text.trim().length > 0
                    onClicked: root.saveManual()
                }
            }

            Item { Layout.fillHeight: true }
        }

        RowLayout
        {
            Layout.fillWidth: true

            DesktopButton
            {
                visible: wordPicker.visible && !root.searching
                text: qsTr("Open in Ikonika")
                onClicked:
                {
                    const entry = lookup.entry
                    root.closeLookup(false)
                    root.openEntryRequested(entry.headword, entry.partOfSpeech)
                }
            }

            DesktopLabel
            {
                Layout.fillWidth: true
                text: root.errorText
                color: AppTheme.colourWrong
            }
        }
    }
}
