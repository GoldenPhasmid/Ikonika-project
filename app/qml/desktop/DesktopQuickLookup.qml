import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Window

Window
{
    id: root

    required property QtObject service
    required property VocabularyController vocabulary
    required property var store

    signal openEntryRequested(string word, string partOfSpeech)
    signal lookUpRequested(string text)

    property real baseWidth: AppMetrics.quickAddWidth
    property real baseHeight: AppMetrics.quickAddHeight
    property bool wasActive: false
    property string pickedWord: ""
    property int pickedPosition: -1
    property bool revealWord: false
    property string errorText: ""

    readonly property TextArea input: queryArea.input
    readonly property string text: root.input.text.trim()
    readonly property bool reverse: lookup.nativeSearch
    readonly property bool wordShown: mainResult.translationShown && root.pickedWord.length > 0
    readonly property DesktopQuickAddResult target: root.wordShown ? wordResult : mainResult
    readonly property string speechText: root.reverse ? ""
        : (root.input.selectedText.trim() || (mainResult.entry ? mainResult.entry.headword : root.text))

    readonly property real fixedHeight: header.height + inputBlock.implicitHeight + AppMetrics.borderWidth * 2
    readonly property real fittedHeight: root.fixedHeight + mainResult.implicitHeight + AppMetrics.gapMedium * 2
    readonly property real targetHeight: mainResult.translationShown
        ? Math.max(root.baseHeight, Math.min(root.service.availableHeight(root), root.fittedHeight))
        : root.baseHeight

    function wordCount(text)
    {
        return text.split(/\s+/).filter(Part => Part.length > 0).length
    }

    function showSelection(text)
    {
        console.info("Quick Add shows a copied selection of", root.wordCount(text), "words")

        root.prepare(text)
    }

    function showTyping()
    {
        console.info("Quick Add opened empty for typing")

        root.prepare("")
    }

    function prepare(text)
    {
        lookup.nativeSearch = false
        root.input.text = text
        root.input.cursorPosition = root.input.length
        root.refresh()
        focusGrace.restart()
        root.service.present(root)
        root.input.forceActiveFocus()
    }

    function closeLookup(restoreFocus)
    {
        console.info("Quick Add closed")

        root.service.saveSize(Math.round(root.baseWidth), Math.round(root.baseHeight))
        root.wasActive = false
        focusGrace.stop()
        searchDelay.stop()
        languageSwitch.close()
        nativeSwitch.close()
        root.service.dismiss(root, restoreFocus)
        root.input.clear()
        root.pickedWord = ""
        mainResult.search()
        wordResult.search()
    }

    function refresh()
    {
        searchDelay.stop()
        root.errorText = ""
        root.pickedWord = ""
        root.pickedPosition = -1
        root.revealWord = false
        bodyArea.contentY = 0
        mainResult.search()
        wordResult.search()
    }

    function openForeign(word)
    {
        console.info("Quick Add opened", word, "in the language learned")

        lookup.nativeSearch = false
        root.input.text = word
        root.input.cursorPosition = root.input.length
        root.refresh()
    }

    function pickFromSelection()
    {
        if (!mainResult.translationShown)
        {
            return
        }

        const picked = root.input.selectedText.trim()
        const words = root.wordCount(picked)

        if (words === 0 || words > AppMetrics.readerLookupWordLimit || picked === root.pickedWord)
        {
            return
        }

        root.pickedWord = picked
        root.pickedPosition = root.input.selectionStart
        root.revealWord = true
        wordResult.search()
    }

    function showWord()
    {
        if (!root.revealWord || !root.wordShown || wordResult.searching)
        {
            return
        }

        if (wordDivider.y > bodyArea.height / 2)
        {
            bodyArea.contentY = Math.max(0, Math.min(wordDivider.y, bodyArea.contentHeight - bodyArea.height))
        }

        root.revealWord = wordResult.entry === null && !wordResult.translationShown
    }

    function add()
    {
        const hint = root.wordShown ? lookup.sentenceAt(root.input.text, Math.max(0, root.pickedPosition)) : ""

        root.errorText = root.target.add(root.vocabulary, hint) ? "" : qsTr("Could not add this word")
    }

    function accept()
    {
        if (root.target.canAdd)
        {
            root.add()

            return
        }

        root.refresh()
    }

    function speak()
    {
        if (root.speechText.length > 0)
        {
            speakButton.speak()
        }
    }

    function openInApp()
    {
        const entry = root.target.entry
        const text = root.wordShown ? root.pickedWord : root.text

        root.closeLookup(false)

        if (entry)
        {
            root.openEntryRequested(entry.headword, entry.partOfSpeech)

            return
        }

        root.lookUpRequested(text)
    }

    title: qsTr("Ikonika Quick Add")
    width: root.baseWidth
    height: root.targetHeight
    minimumWidth: AppMetrics.quickAddMinimumWidth
    minimumHeight: AppMetrics.quickAddMinimumHeight
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    transientParent: null
    color: "transparent"

    Component.onCompleted:
    {
        const saved = root.service.savedSize()

        if (saved.width > 0 && saved.height > 0)
        {
            root.baseWidth = Math.max(AppMetrics.quickAddMinimumWidth, saved.width)
            root.baseHeight = Math.max(AppMetrics.quickAddMinimumHeight, saved.height)
        }
    }

    function restoreSize()
    {
        root.width = Qt.binding(() => root.baseWidth)
        root.height = Qt.binding(() => root.targetHeight)
    }

    onWidthChanged:
    {
        if (Math.round(root.width) < root.minimumWidth)
        {
            root.baseWidth = root.minimumWidth
            Qt.callLater(root.restoreSize)

            return
        }

        if (Math.round(root.width) !== Math.round(root.baseWidth))
        {
            root.baseWidth = root.width
        }
    }

    onHeightChanged:
    {
        if (Math.round(root.height) < root.minimumHeight)
        {
            root.baseHeight = root.minimumHeight
            Qt.callLater(root.restoreSize)

            return
        }

        if (Math.round(root.height) !== Math.round(root.targetHeight))
        {
            root.baseHeight = root.height

            return
        }

        if (root.visible)
        {
            Qt.callLater(root.service.keepOnScreen, root)
        }
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

        if (root.active)
        {
            root.wasActive = true

            return
        }

        if (root.wasActive && !languageSwitch.visible && !nativeSwitch.visible)
        {
            root.closeLookup(false)
        }
    }

    Shortcut
    {
        sequence: "Ctrl+Space"
        onActivated: root.speak()
    }

    Connections
    {
        target: root.service

        function onLookupRequested(text) { root.showSelection(text) }
        function onTypedLookupRequested() { root.showTyping() }

        function onEnabledChanged()
        {
            if (root.visible && !root.service.enabled)
            {
                root.closeLookup(false)
            }
        }
    }

    Connections
    {
        target: root.input

        function onTextChanged()
        {
            if (root.visible)
            {
                searchDelay.restart()
            }
        }

        function onSelectedTextChanged() { root.pickFromSelection() }
    }

    DictionaryController
    {
        id: lookup

        onNativeSearchChanged:
        {
            if (root.visible)
            {
                console.info("Quick Add searches in", lookup.nativeSearch ? "the native language" : "the language learned")
                root.refresh()
            }
        }

        onLanguagesChanged:
        {
            if (root.visible)
            {
                console.info("Quick Add follows the language switch")
                root.refresh()
            }
        }
    }

    DictionaryController
    {
        id: wordLookup
    }

    Timer
    {
        id: focusGrace

        interval: AppMetrics.quickAddFocusGrace
        onTriggered: root.wasActive = root.active
    }

    Timer
    {
        id: searchDelay

        interval: AppMetrics.quickAddSearchDelay
        onTriggered: root.refresh()
    }

    Rectangle
    {
        anchors.fill: parent
        radius: AppMetrics.radiusLarge
        color: AppTheme.colourPanel
        border.width: AppMetrics.borderWidth
        border.color: AppTheme.colourBorder

        Accessible.role: Accessible.Pane
        Accessible.name: root.title

        Keys.onEscapePressed: root.closeLookup(true)

        DesktopResizeFrame
        {
            window: root
        }

        ColumnLayout
        {
            anchors.fill: parent
            anchors.margins: AppMetrics.borderWidth
            spacing: 0

            Item
            {
                id: header

                Layout.fillWidth: true
                implicitHeight: AppMetrics.topBarHeight

                MouseArea
                {
                    anchors.fill: parent

                    onPressed: root.startSystemMove()
                }

                RowLayout
                {
                    anchors.fill: parent
                    anchors.leftMargin: AppMetrics.gapSmall
                    anchors.rightMargin: AppMetrics.gapSmall
                    spacing: AppMetrics.gapTight

                    DesktopIconButton
                    {
                        objectName: "quick_add_app"

                        focusPolicy: Qt.NoFocus
                        imageSource: "qrc:/icons/AppIcon.png"
                        hoveredImageSource: "qrc:/icons/AppIconHover.png"
                        text: qsTr("Open in Ikonika")

                        onClicked: root.openInApp()
                    }

                    DesktopButton
                    {
                        id: languageButton

                        objectName: "quick_add_language"

                        focusPolicy: Qt.NoFocus
                        text: lookup.targetLanguageCode
                        trailingIconName: "chevron_down"

                        Accessible.name: root.store.targetLanguageName
                        Accessible.description: qsTr("Switch the language you learn")

                        onClicked: languageSwitch.visible ? languageSwitch.close() : languageSwitch.open()

                        DesktopLanguagePopover
                        {
                            id: languageSwitch

                            store: root.store
                            installedOnly: true
                            y: languageButton.height + AppMetrics.gapTight
                            width: AppMetrics.settingsControlWidth
                        }
                    }

                    DesktopIconButton
                    {
                        objectName: "quick_add_swap"

                        focusPolicy: Qt.NoFocus
                        iconName: root.reverse ? "arrow_left" : "arrow_right"
                        iconColour: AppTheme.colourMuted
                        text: qsTr("Swap languages")

                        onClicked: lookup.nativeSearch = !lookup.nativeSearch
                    }

                    DesktopLabel
                    {
                        visible: root.store.nativeLanguageCodes.length < 2
                        text: lookup.nativeLanguageCode
                        font.weight: Font.DemiBold
                        color: AppTheme.colourSubtle
                    }

                    DesktopButton
                    {
                        id: nativeButton

                        objectName: "quick_add_native_language"

                        visible: root.store.nativeLanguageCodes.length > 1
                        focusPolicy: Qt.NoFocus
                        text: lookup.nativeLanguageCode
                        trailingIconName: "chevron_down"

                        Accessible.name: root.store.nativeLanguageName
                        Accessible.description: qsTr("Switch your language")

                        onClicked: nativeSwitch.visible ? nativeSwitch.close() : nativeSwitch.open()

                        DesktopNativeLanguagePopover
                        {
                            id: nativeSwitch

                            store: root.store
                            y: nativeButton.height + AppMetrics.gapTight
                            width: AppMetrics.settingsControlWidth
                        }
                    }

                    Item
                    {
                        Layout.fillWidth: true
                    }

                    DesktopSpeakButton
                    {
                        id: speakButton

                        objectName: "quick_add_listen"

                        focusPolicy: Qt.NoFocus
                        enabled: root.speechText.length > 0
                        iconColour: AppTheme.colourMuted
                        text: qsTr("Listen (Ctrl+Space)")

                        onSpeakRequested: lookup.speak(root.speechText)
                    }

                    DesktopIconButton
                    {
                        objectName: "quick_add_copy"

                        visible: mainResult.translated
                        focusPolicy: Qt.NoFocus
                        iconName: "copy"
                        iconColour: AppTheme.colourMuted
                        text: qsTr("Copy translation")

                        onClicked: lookup.copyToClipboard(lookup.machineTranslation)
                    }

                    DesktopButton
                    {
                        objectName: "quick_add_add"

                        visible: root.target.addShown
                        enabled: root.target.canAdd
                        tone: "primary"
                        focusPolicy: Qt.NoFocus
                        iconName: "plus"
                        text: qsTr("Add")

                        Accessible.description: qsTr("Add to My Vocabulary (Enter)")

                        onClicked: root.add()
                    }

                    DesktopIconButton
                    {
                        objectName: "quick_add_close"

                        Layout.leftMargin: AppMetrics.gapSmall
                        focusPolicy: Qt.NoFocus
                        iconName: "close"
                        text: qsTr("Close")

                        onClicked: root.closeLookup(true)
                    }
                }

                DesktopDivider
                {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                }
            }

            Item
            {
                id: inputBlock

                Layout.fillWidth: true
                implicitHeight: queryArea.Layout.preferredHeight + AppMetrics.gapCompact * 2

                DesktopTextArea
                {
                    id: queryArea

                    objectName: "quick_add_text"

                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: AppMetrics.gapCompact
                    height: Layout.preferredHeight

                    growing: true
                    submitOnReturn: true
                    fontSize: AppMetrics.fontLabel
                    accessibleName: qsTr("Word or sentence")
                    placeholderText: qsTr("Type a word or sentence")
                    Layout.preferredHeight: Math.min(queryArea.input.implicitHeight,
                                                     root.service.availableHeight(root) * AppMetrics.quickAddInputShare)

                    onAccepted: root.accept()

                    Component.onCompleted: queryArea.input.persistentSelection = true
                }
            }

            Item
            {
                Layout.fillWidth: true
                Layout.fillHeight: true

                Flickable
                {
                    id: bodyArea

                    anchors.fill: parent
                    anchors.leftMargin: AppMetrics.gapCompact
                    anchors.rightMargin: AppMetrics.gapCompact
                    contentWidth: width
                    contentHeight: bodyColumn.implicitHeight + AppMetrics.gapMedium
                    boundsBehavior: Flickable.StopAtBounds
                    clip: true

                    onMovementStarted: root.revealWord = false

                    ScrollBar.vertical: bodyScrollBar

                    ColumnLayout
                    {
                        id: bodyColumn

                        width: parent.width
                        spacing: AppMetrics.gapMedium

                        DesktopLabel
                        {
                            Layout.fillWidth: true
                            Layout.leftMargin: AppMetrics.gapCompact
                            Layout.rightMargin: AppMetrics.gapCompact
                            visible: root.errorText.length > 0
                            text: root.errorText
                            color: AppTheme.colourWrong
                            wrapMode: Text.WordWrap
                        }

                        DesktopQuickAddResult
                        {
                            id: mainResult

                            Layout.fillWidth: true
                            visible: !root.reverse
                            lookup: lookup
                            store: root.store
                            term: root.text
                            reverse: root.reverse
                            downloadTone: root.target.addShown ? "secondary" : "primary"

                            onWordClicked: function(word) { root.openForeign(word) }
                        }

                        DesktopDivider
                        {
                            id: wordDivider

                            Layout.fillWidth: true
                            visible: root.wordShown
                        }

                        DesktopQuickAddResult
                        {
                            id: wordResult

                            Layout.fillWidth: true
                            visible: root.wordShown
                            lookup: wordLookup
                            store: root.store
                            term: root.pickedWord
                            downloadTone: "secondary"

                            onWordClicked: function(word) { root.openForeign(word) }
                            onImplicitHeightChanged: Qt.callLater(root.showWord)
                        }

                        ColumnLayout
                        {
                            Layout.fillWidth: true
                            visible: root.reverse && root.text.length > 0
                            spacing: 0

                            DesktopLabel
                            {
                                Layout.fillWidth: true
                                Layout.leftMargin: AppMetrics.gapCompact
                                visible: !mainResult.searching && lookup.results.length === 0
                                text: qsTr("Nothing found")
                                color: AppTheme.colourMuted
                            }

                            Repeater
                            {
                                model: root.reverse ? lookup.results : []

                                delegate: DesktopWordRow
                                {
                                    required property var modelData

                                    Layout.fillWidth: true
                                    width: bodyColumn.width
                                    word: modelData.displayWord || modelData.word
                                    translation: modelData.translation || ""
                                    verbGroup: modelData.verbGroup || ""
                                    cefrLevel: modelData.cefrLevel || ""
                                    stage: modelData.stage !== undefined ? modelData.stage : -1
                                    completed: !!modelData.completed
                                    maxStage: root.store.stageCount

                                    onClicked: root.openForeign(modelData.word)
                                }
                            }
                        }
                    }
                }

                DesktopScrollBar
                {
                    id: bodyScrollBar

                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                }
            }
        }
    }
}
