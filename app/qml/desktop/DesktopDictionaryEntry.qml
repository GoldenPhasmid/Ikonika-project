import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQml.Models

Item
{
    id: root

    property string word: ""
    property string searchText: ""
    property string partOfSpeech: ""
    property bool showAdd: true
    property string emptyText: ""
    property int stageCount: 1

    property alias actions: actionRow.data

    signal addWordRequested(string word, string translation, string partOfSpeech, string meaningGist)
    signal editWordRequested(string cardId, string word, string translation, int gender,
                             string partOfSpeech, int box)
    signal wordClicked(string word)

    readonly property var entry: controller.entry.headword ? controller.entry : null
    readonly property string headword: root.entry ? root.entry.headword : root.word

    readonly property var addedTranslation: controller.allTranslations.find(Translation => Translation.learnedStage >= 0) || null

    readonly property string shownPartOfSpeech: root.entry ? root.entry.partOfSpeech : root.partOfSpeech
    readonly property var meanings: root.entry ? (root.entry.meanings || []) : []
    readonly property bool hasMeanings: root.meanings.length > 0

    readonly property var meaningTranslationKeys: root.meanings.reduce(
        (Keys, Meaning) => Keys.concat(Meaning.translations.map(Text => Text.trim().toLowerCase())), [])

    readonly property var otherTranslations: root.hasMeanings
        ? controller.allTranslations.filter(Translation =>
            root.meaningTranslationKeys.indexOf(Translation.text.trim().toLowerCase()) < 0)
        : controller.allTranslations

    function findTranslationRow(text)
    {
        const wanted = text.trim().toLowerCase()

        return controller.allTranslations.find(Translation => Translation.text.trim().toLowerCase() === wanted)
            || ({ text: text, relative: 0, hasProbability: false, learnedStage: -1, cardId: "" })
    }

    function pickTranslation(translation, meaningGist)
    {
        if (translation.learnedStage >= 0 && translation.cardId.length > 0)
        {
            root.editWordRequested(translation.cardId, root.headword, translation.text, translation.cardGender,
                                   translation.cardPartOfSpeech, translation.learnedStage)

            return
        }

        root.addWordRequested(root.headword, translation.text, root.shownPartOfSpeech, meaningGist)
    }

    function open(headword, pos, record, query)
    {
        root.searchText = query || headword
        root.word = headword
        root.partOfSpeech = pos || ""

        controller.openEntry(headword, root.partOfSpeech, root.searchText)

        if (record)
        {
            controller.recordSearch()
        }

        page.contentY = 0
    }

    function clear()
    {
        root.word = ""
        root.partOfSpeech = ""
        controller.openEntry("", "")
    }

    function refresh()
    {
        if (root.word.length > 0)
        {
            controller.openEntry(root.word, root.partOfSpeech, root.searchText)
        }
    }

    function selectPartOfSpeech(pos)
    {
        if (pos === root.entry.partOfSpeech)
        {
            return
        }

        root.partOfSpeech = pos
        controller.openEntry(root.word, pos, root.searchText)
    }

    function editCard(card)
    {
        root.editWordRequested(card.cardId, card.word, card.translation, card.gender, card.partOfSpeech, card.stage)
    }

    function editCards()
    {
        if (controller.cards.length === 1)
        {
            root.editCard(controller.cards[0])

            return
        }

        cardMenu.open()
    }

    function speak()
    {
        if (root.headword.length > 0)
        {
            controller.speak(root.headword)
        }
    }

    Shortcut
    {
        sequence: "Ctrl+Space"
        enabled: root.visible && root.headword.length > 0
        context: Qt.WindowShortcut

        onActivated: entryHeader.speak()
    }

    DictionaryController
    {
        id: controller
    }

    DesktopLabel
    {
        anchors.centerIn: parent
        width: Math.min(parent.width - AppMetrics.contentPadding * 2, AppMetrics.contentWidthNarrow)
        visible: root.word.length === 0
        text: root.emptyText
        font.pixelSize: AppMetrics.fontLabel
        color: AppTheme.colourMuted
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
    }

    DesktopPage
    {
        id: page
        objectName: "dictionary_entry_page"

        anchors.fill: parent
        visible: root.word.length > 0
        maximumWidth: AppMetrics.contentWidthNarrow
        spacing: AppMetrics.gapLarge + AppMetrics.gapSmall

        ColumnLayout
        {
            Layout.fillWidth: true
            spacing: AppMetrics.gapSmall

            DesktopEntryHeader
            {
                id: entryHeader

                headword: root.entry ? root.entry.displayHeadword : root.headword
                gender: root.entry ? root.entry.headwordGender : -1
                verbGroup: root.entry ? root.entry.verbGroup || "" : ""
                verbGroupLabel: root.entry ? root.entry.verbGroupLabel || "" : ""
                meta: root.entry ? root.entry.gender : ""
                phonetic: root.entry ? root.entry.phonetic : ""
                frequencyBucket: root.entry ? root.entry.frequencyBucket : ""
                cefrLevel: root.entry ? root.entry.cefrLevel : ""

                onSpeakRequested: root.speak()
                onWebSearchRequested: controller.openWebSearch(root.headword)

                actions: [
                    RowLayout
                    {
                        id: actionRow

                        Layout.fillWidth: false
                        spacing: AppMetrics.gapSmall
                    },
                    DesktopButton
                    {
                        id: editButton

                        objectName: "entry_edit"
                        visible: root.showAdd && controller.cards.length > 0
                        iconName: "edit"
                        text: qsTr("Edit")

                        onClicked: root.editCards()

                        DesktopMenu
                        {
                            id: cardMenu

                            y: editButton.height

                            Instantiator
                            {
                                model: controller.cards

                                delegate: DesktopMenuItem
                                {
                                    required property var modelData

                                    text: modelData.translation
                                    stage: modelData.stage
                                    completed: modelData.completed

                                    onTriggered: root.editCard(modelData)
                                }

                                onObjectAdded: function(index, object) { cardMenu.insertItem(index, object) }
                                onObjectRemoved: function(index, object) { cardMenu.removeItem(object) }
                            }
                        }
                    },
                    DesktopButton
                    {
                        visible: root.showAdd
                        tone: root.addedTranslation ? "secondary" : "primary"
                        iconName: "plus"
                        text: qsTr("Add word")

                        onClicked: root.addWordRequested(root.headword, "", root.shownPartOfSpeech, "")
                    }
                ]

                status: [
                    DesktopIcon
                    {
                        visible: root.showAdd && root.addedTranslation !== null
                        name: "check"
                        width: AppMetrics.iconSizeSmall
                        height: AppMetrics.iconSizeSmall
                        color: AppTheme.colourCorrect
                    },
                    DesktopLabel
                    {
                        visible: root.showAdd && root.addedTranslation !== null
                        Layout.rightMargin: AppMetrics.gapSmall
                        text: qsTr("Added")
                        font.pixelSize: AppMetrics.fontSmall
                        color: AppTheme.colourCorrect
                    }
                ]
            }

            DictionaryMatchedForm
            {
                Layout.fillWidth: true
                Layout.topMargin: AppMetrics.gapSmall
                form: root.entry ? root.entry.forms.matchedText || "" : ""
                readings: root.entry ? root.entry.forms.matchedReadings || [] : []
                tint: AppTheme.verbGroupColours[root.entry ? root.entry.verbGroup : ""] ?? AppTheme.colourAccent
            }

            DesktopLabel
            {
                Layout.fillWidth: true
                visible: root.entry === null || !root.entry.inDictionary
                text: qsTr("This word is not in the dictionary.")
                color: AppTheme.colourMuted
            }
        }

        Item
        {
            Layout.fillWidth: true
            implicitHeight: tabRow.implicitHeight
            visible: controller.readings.length > 0

            Accessible.role: Accessible.PageTabList
            Accessible.name: qsTr("Parts of speech")

            Rectangle
            {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: AppMetrics.borderWidth
                color: AppTheme.colourBorder
            }

            Row
            {
                id: tabRow

                spacing: AppMetrics.gapLarge

                Repeater
                {
                    model: controller.readings

                    delegate: DesktopTab
                    {
                        required property var modelData

                        text: modelData.label
                        selected: root.entry !== null && modelData.partOfSpeech === root.entry.partOfSpeech
                        stage: modelData.stage
                        completed: modelData.completed

                        onClicked: root.selectPartOfSpeech(modelData.partOfSpeech)
                    }
                }
            }
        }

        ColumnLayout
        {
            Layout.fillWidth: true
            spacing: AppMetrics.gapCompact
            visible: root.hasMeanings

            DesktopSectionLabel { text: qsTr("Meanings") }

            Repeater
            {
                model: root.meanings

                delegate: DesktopDictionaryMeaning
                {
                    required property var modelData
                    required property int index

                    objectName: "dictionary_meaning_" + index

                    meaning: modelData
                    number: index + 1
                    stageCount: root.stageCount
                    collapsible: root.meanings.length >= 3
                    expanded: !collapsible || index === 0
                    columns: page.columnWidth >= AppMetrics.twoColumnMinimumWidth ? 3 : 2
                    translations: modelData.translations.map(Text => root.findTranslationRow(Text))

                    onAddRequested: root.addWordRequested(root.headword, modelData.translations[0] || "",
                                                          root.shownPartOfSpeech, modelData.gist)
                    onEditRequested: root.editWordRequested(modelData.cardId, root.headword, modelData.cardTranslation,
                                                            modelData.cardGender, root.shownPartOfSpeech,
                                                            modelData.learnedStage)
                    onTranslationPicked: function(translation) { root.pickTranslation(translation, modelData.gist) }
                    onWordClicked: function(word) { root.wordClicked(word) }
                }
            }
        }

        ColumnLayout
        {
            Layout.fillWidth: true
            spacing: AppMetrics.gapCompact
            visible: root.otherTranslations.length > 0

            DesktopSectionLabel { text: root.hasMeanings ? qsTr("Other translations") : qsTr("Translations") }

            GridLayout
            {
                Layout.fillWidth: true
                columns: page.columnWidth >= AppMetrics.twoColumnMinimumWidth ? 2 : 1
                columnSpacing: AppMetrics.gapSmall
                rowSpacing: AppMetrics.gapSmall

                Repeater
                {
                    model: root.otherTranslations

                    delegate: DesktopTranslationChip
                    {
                        required property var modelData

                        Layout.preferredWidth: 1
                        text: modelData.text
                        relative: modelData.relative
                        hasProbability: modelData.hasProbability
                        learnedStage: modelData.learnedStage

                        onClicked: root.pickTranslation(modelData, "")
                    }
                }
            }
        }

        ColumnLayout
        {
            Layout.fillWidth: true
            spacing: AppMetrics.gapCompact
            visible: !root.hasMeanings && root.entry !== null && root.entry.senses.length > 0

            DesktopSectionLabel { text: qsTr("Meanings") }

            Repeater
            {
                model: root.entry ? root.entry.senses : []

                delegate: RowLayout
                {
                    id: sense

                    required property var modelData
                    required property int index

                    Layout.fillWidth: true
                    spacing: AppMetrics.gapCompact

                    DesktopLabel
                    {
                        Layout.alignment: Qt.AlignTop
                        Layout.preferredWidth: AppMetrics.listMarkerWidth
                        text: sense.index + 1
                        font.pixelSize: AppMetrics.dictionaryFontBody
                        font.weight: Font.Bold
                        color: AppTheme.colourMuted
                    }

                    ColumnLayout
                    {
                        Layout.fillWidth: true
                        spacing: AppMetrics.gapTight

                        DesktopLabel
                        {
                            Layout.fillWidth: true
                            textFormat: Text.RichText
                            text: (sense.modelData.gender.length > 0 ? "<b>" + sense.modelData.gender + ".</b> " : "")
                                + sense.modelData.text
                            font.pixelSize: AppMetrics.dictionaryFontBody
                            wrapMode: Text.WordWrap
                        }

                        DesktopLabel
                        {
                            Layout.fillWidth: true
                            visible: sense.modelData.translations.length > 0
                            text: sense.modelData.translations.join(", ")
                            font.pixelSize: AppMetrics.fontBody
                            color: AppTheme.colourMuted
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }
        }

        DesktopDictionaryForms
        {
            id: conjugation
            conjugationUrl: root.entry ? root.entry.conjugationUrl || "" : ""
            synonymsUrl: root.entry && root.entry.forms.language && root.entry.headword
                ? "https://synonyms.reverso.net/synonyme/" + encodeURIComponent(root.entry.forms.language)
                    + "/" + encodeURIComponent(root.entry.headword)
                : ""
            forms: root.entry ? root.entry.forms : ({ kind: "none" })
        }

        ColumnLayout
        {
            Layout.fillWidth: true
            spacing: AppMetrics.gapCompact
            visible: root.entry !== null && root.entry.examples.length > 0

            DesktopSectionLabel { text: qsTr("Examples") }

            GridLayout
            {
                Layout.fillWidth: true
                columns: page.columnWidth >= AppMetrics.twoColumnMinimumWidth ? 2 : 1
                uniformCellWidths: true
                columnSpacing: AppMetrics.gapLarge
                rowSpacing: AppMetrics.gapCompact

                Repeater
                {
                    model: root.entry ? root.entry.examples : []

                    delegate: Item
                    {
                        id: example

                        required property var modelData

                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        implicitHeight: exampleColumn.implicitHeight

                        Rectangle
                        {
                            width: AppMetrics.focusRingWidth
                            height: parent.height
                            radius: width / 2
                            color: AppTheme.colourBorder
                        }

                        ColumnLayout
                        {
                            id: exampleColumn

                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.leftMargin: AppMetrics.gapMedium
                            spacing: AppMetrics.gapTight

                            DesktopSentenceText
                            {
                                sentence: example.modelData.text
                                maximumSize: AppMetrics.dictionaryFontBody
                                font.weight: Font.Normal
                                horizontalAlignment: Text.AlignLeft
                                verticalAlignment: Text.AlignTop

                                onWordClicked: function(word) { root.wordClicked(word) }
                            }

                            DesktopLabel
                            {
                                Layout.fillWidth: true
                                visible: text.length > 0
                                text: example.modelData.translation
                                font.italic: true
                                color: AppTheme.colourSubtle
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }
        }
    }
}
