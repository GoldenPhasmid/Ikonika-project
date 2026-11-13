import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item
{
    id: root

    required property var lookup
    property var geometry: null
    property string text: ""
    property string sentence: ""
    property bool darkPaper: false
    property real contentWidth: root.width
    property int maxStage: 1
    property bool selecting: false
    property bool showTrigger: true
    property bool readAloudOffered: false

    property bool open: false
    property bool searching: false
    property bool pending: false

    signal addWordRequested(string word, string translation, string partOfSpeech, string hint)
    signal dictionaryRequested(string word)
    signal readAloudRequested()

    readonly property int words: root.wordCount(root.text)
    readonly property bool brief: root.words > 0 && root.words <= AppMetrics.readerLookupWordLimit
    readonly property var matches: root.open && root.brief && !root.searching ? root.findMatches(root.lookup.results) : []
    readonly property var shownMatches: root.rankMatches(root.matches, contextLookup.machineTranslation)
    readonly property var match: root.shownMatches.length > 0 ? root.shownMatches[0] : null
    readonly property bool cardShown: root.match !== null
    readonly property bool translated: !root.lookup.translating && root.lookup.machineTranslation.length > 0
    readonly property bool downloadShown: !root.cardShown && !root.lookup.translating && !root.translated
                                          && AppMachineTranslation.offered
    readonly property bool actionsShown: !root.downloadShown && !root.lookup.translating
                                         && (root.cardShown || root.translated || root.brief)
    readonly property bool onScreen: root.geometry !== null && root.geometry.endY >= 0 && root.geometry.endY <= root.height
    readonly property bool available: root.brief || AppMachineTranslation.installed || AppMachineTranslation.offered

    function wordCount(text)
    {
        return text.split(/\s+/).filter(part => part.length > 0).length
    }

    function findMatches(results)
    {
        return results.filter(Row => Row.exact)
    }

    function translationStem(phrase)
    {
        if (phrase.length > 5)
        {
            return phrase.slice(0, -2)
        }

        return phrase.length > 3 ? phrase.slice(0, -1) : phrase
    }

    function supportedBy(row, contextText)
    {
        const words = contextText.split(/[\s.,;:!?"()\[\]«»—–-]+/).filter(Word => Word.length > 0)

        return (row.translation || "").split(", ").some(function(Phrase)
        {
            const phrase = Phrase.trim().toLowerCase().replace(/^(to|a|an|the)\s+/, "")

            if (phrase.length === 0)
            {
                return false
            }

            if (phrase.indexOf(" ") >= 0)
            {
                return contextText.indexOf(phrase) >= 0
            }

            const stem = root.translationStem(phrase)

            return words.some(Word => Word === phrase || Word.startsWith(stem))
        })
    }

    function rankMatches(rows, contextTranslation)
    {
        const contextText = contextTranslation.toLowerCase()

        if (rows.length < 2 || contextText.length === 0)
        {
            return rows
        }

        const supported = rows.filter(Row => root.supportedBy(Row, contextText))

        if (supported.length === 0 || supported.length === rows.length)
        {
            return rows
        }

        return supported.concat(rows.filter(Row => supported.indexOf(Row) < 0))
    }

    function expect()
    {
        root.pending = true
    }

    function show()
    {
        root.pending = false
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

    function dismiss()
    {
        if (!root.open)
        {
            return false
        }

        root.open = false

        return true
    }

    function containsPopup(position)
    {
        return box.visible && box.contains(box.mapFromItem(root, position))
    }

    function addMatch(row)
    {
        root.addWordRequested(row.word, (row.translation || "").split(", ")[0], row.partOfSpeech || "", root.sentence)
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
        contextLookup.translate("")

        if (root.matches.length > 1 && root.sentence.trim().length > 0)
        {
            console.info("Translating the sentence to rank", root.matches.length, "dictionary matches")
            contextLookup.translate(root.sentence)
        }
    }

    onTextChanged:
    {
        root.open = false
        root.searching = false
        contextLookup.translate("")
    }
    onGeometryChanged: if (root.geometry === null) root.open = false

    Connections
    {
        target: AppMachineTranslation

        function onChanged()
        {
            if (AppMachineTranslation.installed)
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

            if (root.lookup.results.length > 0 && root.findMatches(root.lookup.results).length === 0)
            {
                root.lookup.translate(root.text)

                return
            }

            root.translateContext()
        }
    }

    DictionaryController
    {
        id: contextLookup
    }

    DesktopButton
    {
        id: trigger

        objectName: "reader_translate"

        visible: root.showTrigger && !root.open && !root.pending && !root.selecting && root.onScreen && root.available
        x: Math.max(AppMetrics.gapSmall, Math.min(root.width - trigger.width - AppMetrics.gapSmall, root.geometry ? root.geometry.endX : 0))
        y: root.geometry ? (root.geometry.bottom + trigger.height + AppMetrics.gapSmall <= root.height
            ? root.geometry.bottom + AppMetrics.gapSmall
            : Math.max(AppMetrics.gapSmall, root.geometry.top - trigger.height - AppMetrics.gapSmall)) : 0
        focusPolicy: Qt.NoFocus
        iconName: "languages"
        text: qsTr("Translate")

        background: Rectangle
        {
            radius: AppMetrics.radiusMedium
            color: AppTheme.colourPanel
            border.width: AppMetrics.borderWidth
            border.color: AppTheme.colourBorder

            Rectangle
            {
                anchors.fill: parent
                anchors.margins: AppMetrics.borderWidth
                radius: parent.radius - AppMetrics.borderWidth
                color: trigger.down ? AppTheme.colourPressed : (trigger.hovered ? AppTheme.colourHover : "transparent")

                Behavior on color { ColorAnimation { duration: AppMetrics.motionFast } }
            }
        }

        onClicked: root.show()
    }

    Rectangle
    {
        id: box

        objectName: "reader_translation"

        readonly property real below: root.geometry ? root.geometry.bottom + AppMetrics.gapSmall : 0
        readonly property real above: root.geometry ? root.geometry.top - AppMetrics.gapSmall - box.height : 0
        readonly property real minimumWidth: root.brief ? AppMetrics.popoverWidth
                                                        : root.contentWidth * AppMetrics.readerTranslationWidthShare
        readonly property real iconInset: (AppMetrics.iconSize - AppMetrics.headerIconTapWidth) / 2
        readonly property real cardInset: AppMetrics.gapTight - AppMetrics.gapMedium
        readonly property bool bare: root.downloadShown && !AppMachineTranslation.downloading
        readonly property real padding: box.bare ? 0 : AppMetrics.gapMedium

        visible: root.open && root.onScreen && !root.searching
        width: box.bare ? download.fullWidth
                        : Math.min(root.width - AppMetrics.gapMedium * 2,
                                   Math.max(box.minimumWidth, root.brief || !root.geometry ? 0 : root.geometry.width))
        height: content.implicitHeight + box.padding * 2
        x: Math.max(AppMetrics.gapMedium, Math.min(root.width - box.width - AppMetrics.gapMedium, root.geometry ? root.geometry.left : 0))
        y: box.below + box.height <= root.height - AppMetrics.gapMedium || box.above < AppMetrics.gapMedium ? box.below : box.above

        radius: AppMetrics.radiusLarge
        color: box.bare ? "transparent" : root.darkPaper ? AppTheme.colourCard : AppTheme.colourPanel
        border.width: box.bare ? 0 : AppMetrics.borderWidth
        border.color: root.darkPaper ? AppTheme.colourOutline : AppTheme.colourBorder

        Accessible.role: Accessible.Pane
        Accessible.name: qsTr("Translation")

        onVisibleChanged: if (box.visible) entrance.restart()

        ParallelAnimation
        {
            id: entrance

            NumberAnimation { target: box; property: "opacity"; from: 0; to: 1; duration: AppMetrics.motionFast }
            NumberAnimation { target: rise; property: "y"; from: -AppMetrics.gapTight; to: 0; duration: AppMetrics.motionFast; easing.type: Easing.OutCubic }
        }

        transform: Translate { id: rise }

        MouseArea
        {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
        }

        ColumnLayout
        {
            id: content

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: box.padding
            spacing: AppMetrics.gapSmall

            ColumnLayout
            {
                Layout.fillWidth: true
                visible: root.cardShown
                spacing: 0

                Repeater
                {
                    model: root.shownMatches

                    delegate: DesktopWordRow
                    {
                        required property var modelData
                        required property int index

                        objectName: "reader_translation_card_" + index

                        Layout.fillWidth: true
                        Layout.leftMargin: box.cardInset
                        Layout.rightMargin: box.cardInset
                        Layout.topMargin: index === 0 ? box.cardInset : 0

                        word: modelData.displayWord || modelData.word
                        translation: modelData.translation || ""
                        partOfSpeech: root.shownMatches.length > 1 ? (modelData.partOfSpeechLabel || "") : ""
                        verbGroup: modelData.verbGroup || ""
                        stage: modelData.stage !== undefined ? modelData.stage : -1
                        completed: !!modelData.completed
                        maxStage: root.maxStage

                        onClicked: root.addMatch(modelData)
                    }
                }
            }

            DesktopMachineTranslation
            {
                objectName: "reader_translation_form"

                lookup: root.lookup
                term: root.text
                rowInset: box.cardInset
                visible: root.cardShown && root.lookup.formTerm.length > 0

                onAddRequested: function(word, translation)
                {
                    root.addWordRequested(word, translation, "phrase", root.sentence)
                }
            }

            DesktopLabel
            {
                objectName: "reader_translation_text"

                Layout.fillWidth: true
                visible: !root.cardShown && !root.downloadShown
                text: root.lookup.translating ? qsTr("Translating")
                    : root.translated ? root.lookup.machineTranslation
                    : root.brief ? qsTr("Not in the dictionary") : qsTr("No translation")
                color: root.translated ? AppTheme.colourText : AppTheme.colourMuted
                font.pixelSize: AppMetrics.fontLead
                wrapMode: Text.WordWrap
                textFormat: Text.PlainText
            }

            DesktopTranslationDownload
            {
                id: download

                buttonName: "reader_translation_download"

                Layout.fillWidth: true
                visible: root.downloadShown
            }

            RowLayout
            {
                Layout.fillWidth: true
                Layout.leftMargin: box.iconInset + (root.cardShown ? AppMetrics.gapTight : 0)
                Layout.topMargin: box.iconInset
                Layout.bottomMargin: box.iconInset
                visible: root.actionsShown
                spacing: 0

                DesktopIconButton
                {
                    objectName: "reader_translation_listen"

                    focusPolicy: Qt.NoFocus
                    iconName: "speaker_on"
                    iconColour: AppTheme.colourMuted
                    text: qsTr("Listen")

                    onClicked: root.lookup.speak(root.cardShown ? root.match.word : root.text)
                }

                DesktopIconButton
                {
                    objectName: "reader_translation_read_aloud"

                    visible: root.readAloudOffered
                    focusPolicy: Qt.NoFocus
                    iconName: "headphones"
                    iconColour: AppTheme.colourMuted
                    text: qsTr("Read aloud from here")

                    onClicked: root.readAloudRequested()
                }

                DesktopIconButton
                {
                    objectName: "reader_translation_copy"

                    visible: !root.cardShown && root.translated
                    focusPolicy: Qt.NoFocus
                    iconName: "copy"
                    iconColour: AppTheme.colourMuted
                    text: qsTr("Copy translation")

                    onClicked: root.lookup.copyToClipboard(root.lookup.machineTranslation)
                }

                DesktopIconButton
                {
                    objectName: "reader_translation_dictionary"

                    visible: root.cardShown
                    focusPolicy: Qt.NoFocus
                    iconName: "nav_words"
                    iconColour: AppTheme.colourMuted
                    text: qsTr("Open in dictionary")

                    onClicked: root.dictionaryRequested(root.match.word)
                }

                DesktopIconButton
                {
                    objectName: "reader_translation_add"

                    visible: root.brief || root.translated
                    focusPolicy: Qt.NoFocus
                    iconName: "plus"
                    iconColour: AppTheme.colourMuted
                    text: qsTr("Add to My Vocabulary")

                    onClicked: root.addWord()
                }

                Item
                {
                    Layout.fillWidth: true
                }

                DesktopLabel
                {
                    Layout.rightMargin: -box.iconInset
                    visible: root.translated && !root.cardShown && AppMachineTranslation.attribution.length > 0
                    text: AppMachineTranslation.attribution
                    font.pixelSize: AppMetrics.fontSmall
                    color: AppTheme.colourSubtle
                    elide: Text.ElideRight
                }
            }
        }
    }
}
