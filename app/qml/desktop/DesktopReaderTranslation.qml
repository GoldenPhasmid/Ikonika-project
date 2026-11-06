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

    property bool open: false
    property bool searching: false

    signal addWordRequested(string word, string translation, string partOfSpeech, string hint)
    signal dictionaryRequested(string word)

    readonly property int words: root.wordCount(root.text)
    readonly property bool brief: root.words > 0 && root.words <= AppMetrics.readerLookupWordLimit
    readonly property var match: root.open && root.brief && !root.searching ? root.findMatch(root.lookup.results) : null
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

    function findMatch(results)
    {
        for (const row of results)
        {
            if (row.exact)
            {
                return row
            }
        }

        return null
    }

    function show()
    {
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

    function addWord()
    {
        if (root.match !== null)
        {
            root.addWordRequested(root.match.word, (root.match.translation || "").split(", ")[0],
                                  root.match.partOfSpeech || "", root.sentence)

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

    onTextChanged:
    {
        root.open = false
        root.searching = false
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

            if (root.lookup.results.length > 0 && root.findMatch(root.lookup.results) === null)
            {
                root.lookup.translate(root.text)
            }
        }
    }

    DesktopButton
    {
        id: trigger

        objectName: "reader_translate"

        visible: !root.open && root.onScreen && root.available
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

            DesktopWordRow
            {
                objectName: "reader_translation_card"

                Layout.fillWidth: true
                Layout.leftMargin: box.cardInset
                Layout.rightMargin: box.cardInset
                Layout.topMargin: box.cardInset
                visible: root.cardShown

                word: root.match ? root.match.displayWord || root.match.word : ""
                translation: root.match ? root.match.translation || "" : ""
                cefrLevel: root.match ? root.match.cefrLevel || "" : ""
                verbGroup: root.match ? root.match.verbGroup || "" : ""
                stage: root.match && root.match.stage !== undefined ? root.match.stage : -1
                completed: root.match ? !!root.match.completed : false
                maxStage: root.maxStage

                onClicked: root.dictionaryRequested(root.match.word)
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

            DesktopButton
            {
                id: download

                objectName: "reader_translation_download"

                Layout.fillWidth: true
                visible: root.downloadShown && !AppMachineTranslation.downloading
                focusPolicy: Qt.NoFocus
                tone: "primary"
                iconName: "cloud_download"
                text: AppMachineTranslation.bytes > 0
                    ? qsTr("Download translation · %1 MB").arg(Math.round(AppMachineTranslation.bytes / (1024 * 1024)))
                    : qsTr("Download translation")

                onClicked: AppMachineTranslation.download()
            }

            RowLayout
            {
                Layout.fillWidth: true
                visible: root.downloadShown && AppMachineTranslation.downloading
                spacing: AppMetrics.gapSmall

                DesktopProgressBar
                {
                    Layout.fillWidth: true
                    fill: AppTheme.colourAccent
                    progress: AppMachineTranslation.progress
                }

                DesktopLabel
                {
                    text: qsTr("%1%").arg(Math.round(AppMachineTranslation.progress * 100))
                    font.pixelSize: AppMetrics.fontSmall
                    color: AppTheme.colourMuted
                }
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
