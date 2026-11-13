import QtQuick
import QtQuick.Layouts
import "MobileIcons.js" as Icons

Item
{
    id: root

    property string word: ""
    property string partOfSpeech: ""
    property string searchText: ""
    property int stageCount: 1

    signal done()
    signal addWordRequested(string word, string translation, string partOfSpeech, string meaningGist)
    signal editWordRequested(string cardId, string word, string translation, int gender,
                             string partOfSpeech, int box)
    signal tabSelected(string key)
    signal searchRequested()
    signal wordTapped(string word)

    readonly property var entry: controller.entry.headword ? controller.entry : null
    readonly property string phonetic: !root.entry || root.entry.phonetic.length === 0 ? ""
        : (root.entry.phonetic.startsWith("/") || root.entry.phonetic.startsWith("[")
           ? root.entry.phonetic : qsTr("/%1/").arg(root.entry.phonetic))

    property bool cardSheetOpen: false
    property bool conjugationOpen: false

    readonly property string shownPartOfSpeech: root.entry ? root.entry.partOfSpeech : root.partOfSpeech
    readonly property var meanings: root.entry ? (root.entry.meanings || []) : []
    readonly property bool hasMeanings: root.meanings.length > 0

    readonly property var meaningTranslationKeys: root.meanings.reduce(function(keys, item)
    {
        return keys.concat(item.translations.map(function(text) { return text.trim().toLowerCase() }))
    }, [])

    readonly property var otherTranslations: root.hasMeanings
        ? controller.allTranslations.filter(function(translation)
        {
            return root.meaningTranslationKeys.indexOf(translation.text.trim().toLowerCase()) < 0
        })
        : controller.allTranslations

    function findTranslationRow(text)
    {
        const wanted = text.trim().toLowerCase()

        return controller.allTranslations.find(function(translation)
        {
            return translation.text.trim().toLowerCase() === wanted
        }) || ({ text: text, relative: 0, hasProbability: false, learnedStage: -1, cardId: "" })
    }

    function pickTranslation(translation, meaningGist)
    {
        if (translation.learnedStage >= 0 && translation.cardId.length > 0)
        {
            root.editWordRequested(translation.cardId, root.word, translation.text, translation.cardGender,
                                   translation.cardPartOfSpeech, translation.learnedStage)

            return
        }

        root.addWordRequested(root.word, translation.text, root.shownPartOfSpeech, meaningGist)
    }

    function open(headword, pos, query)
    {
        root.conjugationOpen = false
        formsSection.picker = ""
        root.word = headword
        root.partOfSpeech = pos

        root.searchText = query || ""
        controller.openEntry(headword, pos, root.searchText)
        controller.recordSearch()
        controller.speakAutomatically(headword)
    }

    function selectPartOfSpeech(pos)
    {
        if (root.entry && pos === root.entry.partOfSpeech)
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

        root.cardSheetOpen = true
    }

    function refreshLearnedMarks()
    {
        if (root.word.length > 0)
        {
            controller.openEntry(root.word, root.partOfSpeech)
        }
    }

    DictionaryController
    {
        id: controller
    }

    MobileShell
    {
        id: shell

        anchors.fill: parent

        activeTab: "dictionary"
        headerTitle: root.entry ? root.entry.displayHeadword : root.word
        showBack: true
        showSearch: true
        showTabBar: false

        onBackRequested: root.done()
        onTabSelected: function(key) { root.tabSelected(key) }
        onSearchRequested: root.searchRequested()
        onTrailingActivated: controller.copyToClipboard(root.entry ? root.entry.headword : root.word)

        Flickable
        {
            anchors.fill: parent
            anchors.margins: AppMetrics.contentPadding

            contentWidth: width
            contentHeight: content.implicitHeight
            clip: true

            ColumnLayout
            {
                id: content

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                spacing: AppMetrics.gapLarge

                MobileCard
                {
                    Layout.fillWidth: true

                    RowLayout
                    {
                        Layout.fillWidth: true

                        Text
                        {
                            Layout.fillWidth: true

                            text: root.entry ? root.entry.displayHeadword : root.word
                            font.pixelSize: AppMetrics.dictionaryFontHeadline
                            font.weight: Font.DemiBold
                            font.family: AppMetrics.fontFamily
                            color: AppTheme.verbGroupColours[root.entry ? root.entry.verbGroup : ""] ?? AppTheme.genderColours[root.entry ? root.entry.headwordGender : -1] ?? AppTheme.colourText
                            elide: Text.ElideRight
                        }

                        RowLayout
                        {
                            spacing: AppMetrics.gapSmall

                            MobileActionIconButton
                            {
                                iconSvg: Icons.Icons.plus.path
                                onClicked: root.addWordRequested(root.word, "", root.shownPartOfSpeech, "")
                            }

                            MobileActionIconButton
                            {
                                visible: controller.cards.length > 0
                                iconSvg: Icons.Icons.edit.path
                                onClicked: root.editCards()
                            }

                            MobileActionIconButton
                            {
                                iconSvg: Icons.Icons.speaker_on.path
                                onClicked: controller.speak(root.entry ? root.entry.headword : root.word)
                            }
                        }
                    }

                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: AppMetrics.gapSmall

                        Text
                        {
                            text: root.entry ? root.entry.gender : ""
                            visible: text.length > 0
                            font.pixelSize: AppMetrics.fontSmall
                            font.family: AppMetrics.fontFamily
                            color: AppTheme.colourSubtle
                        }

                        Text
                        {
                            text: root.phonetic
                            visible: text.length > 0
                            font.pixelSize: AppMetrics.fontSmall
                            font.family: AppMetrics.fontFamily
                            color: AppTheme.colourMuted
                        }

                        Item { Layout.fillWidth: true }

                        MobileFrequencyBadge
                        {
                            visible: root.entry !== null && root.entry.frequencyBucket.length > 0
                            bucket: root.entry ? root.entry.frequencyBucket : ""
                        }

                        MobileCefrBadge
                        {
                            visible: root.entry !== null && root.entry.cefrLevel.length > 0
                            level: root.entry ? root.entry.cefrLevel : ""
                        }
                    }

                    Text
                    {
                        Layout.fillWidth: true
                        visible: text.length > 0
                        text: root.entry ? root.entry.verbGroupLabel || "" : ""
                        font.pixelSize: AppMetrics.fontSmall
                        font.family: AppMetrics.fontFamily
                        font.weight: Font.Medium
                        color: AppTheme.verbGroupColours[root.entry ? root.entry.verbGroup : ""] ?? AppTheme.colourSubtle
                    }

                    DictionaryMatchedForm
                    {
                        Layout.fillWidth: true
                        Layout.topMargin: AppMetrics.gapSmall
                        form: root.entry ? root.entry.forms.matchedText || "" : ""
                        readings: root.entry ? root.entry.forms.matchedReadings || [] : []
                        tint: AppTheme.verbGroupColours[root.entry ? root.entry.verbGroup : ""] ?? AppTheme.colourAccent
                    }

                    Text
                    {
                        Layout.fillWidth: true

                        visible: root.entry === null || !root.entry.inDictionary
                        text: qsTr("This word is not in the dictionary.")
                        font.pixelSize: AppMetrics.fontSmall
                        font.family: AppMetrics.fontFamily
                        color: AppTheme.colourMuted
                        wrapMode: Text.WordWrap
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

                    Flow
                    {
                        id: tabRow

                        width: parent.width
                        spacing: AppMetrics.gapLarge

                        Repeater
                        {
                            model: controller.readings

                            delegate: MobileTab
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
                    spacing: AppMetrics.gapMedium

                    visible: root.hasMeanings

                    MobileSectionLabel { text: qsTr("MEANINGS") }

                    Repeater
                    {
                        model: root.meanings

                        delegate: MobileDictionaryMeaning
                        {
                            required property var modelData
                            required property int index

                            stageCount: root.stageCount
                            meaning: modelData
                            number: index + 1
                            collapsible: root.meanings.length > 1
                            expanded: index === 0
                            translations: modelData.translations.map(function(text) { return root.findTranslationRow(text) })

                            onAddRequested: root.addWordRequested(root.word, modelData.translations[0] || "",
                                                                  root.shownPartOfSpeech, modelData.gist)
                            onEditRequested: root.editWordRequested(modelData.cardId, root.word,
                                                                    modelData.cardTranslation, modelData.cardGender,
                                                                    root.shownPartOfSpeech, modelData.learnedStage)
                            onTranslationPicked: function(translation) { root.pickTranslation(translation, modelData.gist) }
                            onWordTapped: function(word) { root.wordTapped(word) }
                        }
                    }
                }

                ColumnLayout
                {
                    Layout.fillWidth: true
                    spacing: 10

                    visible: root.otherTranslations.length > 0

                    MobileSectionLabel { text: root.hasMeanings ? qsTr("OTHER TRANSLATIONS") : qsTr("TRANSLATIONS") }

                    ColumnLayout
                    {
                        Layout.fillWidth: true
                        spacing: AppMetrics.gapSmall

                        Repeater
                        {
                            model: root.otherTranslations

                            delegate: MobileTranslationChip
                            {
                                required property var modelData

                                text: modelData.text
                                relative: modelData.relative
                                hasProbability: modelData.hasProbability
                                learnedStage: modelData.learnedStage

                                onPicked: root.pickTranslation(modelData, "")
                            }
                        }
                    }
                }

                ColumnLayout
                {
                    Layout.fillWidth: true
                    spacing: AppMetrics.gapMedium

                    visible: !root.hasMeanings && root.entry !== null && root.entry.senses.length > 0

                    Repeater
                    {
                        model: root.entry ? root.entry.senses : []

                        delegate: MobileCard
                        {
                            id: senseCard

                            required property var modelData
                            required property int index

                            Layout.fillWidth: true

                            RowLayout
                            {
                                Layout.fillWidth: true
                                spacing: AppMetrics.gapCompact

                                Text
                                {
                                    Layout.alignment: Qt.AlignTop

                                    text: (senseCard.index + 1).toString()
                                    font.pixelSize: AppMetrics.fontBody
                                    font.weight: Font.Bold
                                    font.family: AppMetrics.fontFamily
                                    color: AppTheme.colourMuted
                                }

                                ColumnLayout
                                {
                                    Layout.fillWidth: true
                                    spacing: 6

                                    Text
                                    {
                                        Layout.fillWidth: true

                                        textFormat: Text.RichText
                                        text: (senseCard.modelData.gender.length > 0
                                                ? "<b>" + senseCard.modelData.gender + ". </b>" : "")
                                            + senseCard.modelData.text
                                        font.pixelSize: AppMetrics.fontBody
                                        font.family: AppMetrics.fontFamily
                                        color: AppTheme.colourText
                                        wrapMode: Text.WordWrap
                                    }

                                    Text
                                    {
                                        Layout.fillWidth: true

                                        text: senseCard.modelData.translations.join(", ")
                                        visible: senseCard.modelData.translations.length > 0
                                        font.pixelSize: AppMetrics.fontMeta
                                        font.family: AppMetrics.fontFamily
                                        color: AppTheme.colourMuted
                                        wrapMode: Text.WordWrap
                                    }
                                }
                            }
                        }
                    }
                }

                MobileOptionListPanel
                {
                    Layout.fillWidth: true
                    visible: root.entry !== null && root.entry.forms.kind === "verb"

                    MobileSettingsRow
                    {
                        title: qsTr("Conjugation")
                        lastRow: true
                        onClicked: root.conjugationOpen = true
                    }
                }

                ColumnLayout
                {
                    Layout.fillWidth: true
                    spacing: 10

                    visible: root.entry !== null && root.entry.examples.length > 0

                    MobileSectionLabel { text: qsTr("EXAMPLES") }

                    ColumnLayout
                    {
                        Layout.fillWidth: true
                        spacing: AppMetrics.gapMedium

                        Repeater
                        {
                            model: root.entry ? root.entry.examples : []

                            delegate: MobileCard
                            {
                                id: exampleCard

                                required property var modelData

                                Layout.fillWidth: true
                                spacing: AppMetrics.gapTight

                                MobileSentenceText
                                {
                                    sentence: exampleCard.modelData.text
                                    font.pixelSize: AppMetrics.fontBody
                                    font.weight: Font.Normal
                                    horizontalAlignment: Text.AlignLeft
                                    holdEnabled: false
                                    tapEnabled: true

                                    onWordTapped: function(word) { root.wordTapped(word) }
                                }

                                Text
                                {
                                    Layout.fillWidth: true

                                    text: exampleCard.modelData.translation
                                    visible: text.length > 0
                                    font.pixelSize: AppMetrics.fontSmall
                                    font.italic: true
                                    font.family: AppMetrics.fontFamily
                                    color: AppTheme.colourSubtle
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }
                }
            }
        }

        MobileSheetLoader
        {
            anchors.fill: parent
            z: 10
            open: root.cardSheetOpen

            sourceComponent: MobileCardPickSheet
            {
                cards: controller.cards

                onDismissed: root.cardSheetOpen = false

                onChosen: function(card)
                {
                    root.cardSheetOpen = false
                    root.editCard(card)
                }
            }
        }
    }
    function closeConjugation()
    {
        if (!root.conjugationOpen)
        {
            return false
        }

        if (formsSection.picker === "tense")
        {
            formsSection.picker = "category"
        }
        else if (formsSection.picker !== "")
        {
            formsSection.picker = ""
        }
        else
        {
            root.conjugationOpen = false
        }

        return true
    }

    MobileShell
    {
        anchors.fill: parent
        visible: root.conjugationOpen
        activeTab: "dictionary"
        headerTitle: qsTr("Conjugation")
        showBack: true
        showTabBar: false

        onBackRequested: root.closeConjugation()

        Flickable
        {
            anchors.fill: parent
            anchors.margins: AppMetrics.contentPadding
            contentWidth: width
            contentHeight: conjugationContent.implicitHeight
            clip: true

            ColumnLayout
            {
                id: conjugationContent

                width: parent.width
                spacing: AppMetrics.gapMedium

                Text
                {
                    Layout.fillWidth: true
                    text: root.entry ? root.entry.displayHeadword : root.word
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                    color: AppTheme.colourText
                    font.family: AppMetrics.fontFamily
                    font.pixelSize: AppMetrics.fontBody * 1.5
                    font.weight: Font.DemiBold
                }

                MobileDictionaryFormsSection
                {
                    id: formsSection
                    forms: root.entry ? root.entry.forms : ({ kind: "none" })
                }
            }
        }
    }

    MobileSheetLoader
    {
        anchors.fill: parent
        open: formsSection.picker !== ""

        sourceComponent: MobileBottomSheet
        {
            id: conjugationPicker

            readonly property bool choosingTense: formsSection.picker === "tense"
            readonly property var pending: formsSection.categories[formsSection.pendingCategory]
            readonly property var options: choosingTense && pending ? pending.options : formsSection.categories

            heading: choosingTense && pending ? pending.label : qsTr("Choose a category")
            onDismissed: formsSection.picker = ""

            Flickable
            {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(pickerContent.implicitHeight, conjugationPicker.maximumBodyHeight)
                contentWidth: width
                contentHeight: pickerContent.implicitHeight
                clip: true

                MobileOptionListPanel
                {
                    id: pickerContent
                    width: parent.width

                    MobileSettingsRow
                    {
                        visible: conjugationPicker.choosingTense
                        title: qsTr("‹ Categories")
                        onClicked: formsSection.picker = "category"
                    }

                    Repeater
                    {
                        model: conjugationPicker.options

                        delegate: MobileSettingsRow
                        {
                            required property var modelData
                            required property int index

                            title: modelData.label || ""
                            lastRow: index === conjugationPicker.options.length - 1

                            onClicked:
                            {
                                if (conjugationPicker.choosingTense)
                                {
                                    formsSection.choose(formsSection.pendingCategory, index)
                                }
                                else if (modelData.options.length === 1)
                                {
                                    formsSection.choose(index, 0)
                                }
                                else
                                {
                                    formsSection.pendingCategory = index
                                    formsSection.picker = "tense"
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
