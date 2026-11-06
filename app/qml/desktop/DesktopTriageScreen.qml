import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

FocusScope
{
    id: root

    required property var store
    property var controller: null

    signal leaveRequested()
    signal dictionaryRequested(string word, string partOfSpeech)

    property string pickedGist: ""
    property string meaningTranslation: ""
    property string openGist: ""
    property int highlightedTranslation: -1

    readonly property DesktopScreenMorph morph: screenMorph
    readonly property bool speechOn: !!root.store && root.store.audioEnabled
    readonly property bool muted: !root.speechOn || !root.store.autoSpeak
    readonly property bool hasWord: !!root.controller && root.controller.word.length > 0
    readonly property var options: root.controller ? root.controller.translationOptions : []
    readonly property var meanings: root.controller ? root.controller.meaningOptions : []
    readonly property bool hasMeanings: root.meanings.length > 0
    readonly property string enteredTranslation: translationField.text.trim()
    readonly property var entry: dictionary.entry.headword ? dictionary.entry : null

    readonly property var pickedTranslations: root.meaningTranslation.split(",")
        .map(Part => Part.trim().toLowerCase())
        .filter(Part => Part.length > 0)

    readonly property int defaultOpenIndex: Math.max(0, root.meanings.findIndex(Meaning => Meaning.learnedStage < 0 && Meaning.translations.length > 0))
    readonly property int pickedOpenIndex: root.meanings.findIndex(Meaning => Meaning.gist === root.openGist)
    readonly property int openIndex: root.pickedOpenIndex >= 0 ? root.pickedOpenIndex : (root.hasMeanings ? root.defaultOpenIndex : -1)
    readonly property var openMeaning: root.openIndex >= 0 ? root.meanings[root.openIndex] : null
    readonly property bool keysOnMeanings: root.visible && root.hasMeanings && !translationField.activeFocus

    readonly property bool canLearn: root.hasWord && root.enteredTranslation.length > 0
                                     && (!root.hasMeanings || root.pickedGist.length > 0)

    function learn()
    {
        if (!root.canLearn)
        {
            return
        }

        if (root.hasMeanings)
        {
            root.controller.learnMeaning(root.pickedGist, root.enteredTranslation)

            return
        }

        root.controller.markUnknown()
    }

    function skip()
    {
        if (root.hasWord)
        {
            root.controller.markKnown()
        }
    }

    function pickTranslation(gist, text)
    {
        if (root.pickedGist !== gist)
        {
            root.pickedGist = gist
            root.meaningTranslation = text

            return
        }

        const wanted = text.trim().toLowerCase()
        const kept = root.meaningTranslation.split(",")
            .map(Part => Part.trim())
            .filter(Part => Part.length > 0 && Part.toLowerCase() !== wanted)

        if (kept.length === root.pickedTranslations.length)
        {
            kept.push(text)
        }

        root.meaningTranslation = kept.join(", ")

        if (kept.length === 0)
        {
            root.pickedGist = ""
        }
    }

    function expandMeaning(index)
    {
        if (index < 0 || index >= root.meanings.length)
        {
            return
        }

        root.openGist = root.meanings[index].gist
        root.highlightedTranslation = -1

        Qt.callLater(root.revealMeaning, index)
    }

    function revealMeaning(index)
    {
        const item = meaningRepeater.itemAt(index)

        if (!item)
        {
            return
        }

        let top = meaningRepeater.itemAt(0).y

        for (let previous = 0; previous < index; ++previous)
        {
            top += meaningRepeater.itemAt(previous).targetHeight + meaningColumn.spacing
        }

        top = index === 0 ? 0 : top

        const bottom = Math.min(top + item.targetHeight + meaningFade.height, meaningArea.contentHeight)

        if (top < meaningArea.contentY)
        {
            meaningArea.contentY = top
        }
        else if (bottom > meaningArea.contentY + meaningArea.height)
        {
            meaningArea.contentY = Math.min(top, bottom - meaningArea.height)
        }
    }

    function moveHighlight(step)
    {
        const count = root.openMeaning ? root.openMeaning.translations.length : 0

        if (count === 0)
        {
            return
        }

        root.highlightedTranslation = root.highlightedTranslation < 0 ? (step > 0 ? 0 : count - 1)
                                                                       : Math.max(0, Math.min(count - 1, root.highlightedTranslation + step))
    }

    function pickHighlighted()
    {
        if (!root.openMeaning || root.openMeaning.learnedStage >= 0 || root.highlightedTranslation < 0)
        {
            return
        }

        root.pickTranslation(root.openMeaning.gist, root.openMeaning.translations[root.highlightedTranslation])
    }

    function translationRow(text)
    {
        const wanted = text.trim().toLowerCase()

        return root.options.find(Option => Option.text.trim().toLowerCase() === wanted)
            || ({ text: text, relative: 0, hasProbability: false, learnedStage: -1 })
    }

    function resetWord()
    {
        root.pickedGist = ""
        root.meaningTranslation = ""
        root.openGist = ""
        root.highlightedTranslation = -1
        meaningArea.contentY = 0

        dictionary.openEntry(root.hasWord ? root.controller.word : "",
                             root.hasWord ? root.controller.partOfSpeech : "")

        if (root.hasMeanings)
        {
            const first = root.meanings.findIndex(Meaning => Meaning.learnedStage < 0 && Meaning.translations.length > 0)

            root.expandMeaning(Math.max(0, first))

            if (first >= 0)
            {
                root.pickTranslation(root.meanings[first].gist, root.meanings[first].translations[0])
            }

            meaningArea.forceActiveFocus()

            return
        }

        translationField.forceActiveFocus()
    }

    onVisibleChanged: if (root.visible) Qt.callLater(root.resetWord)

    Connections
    {
        target: root.controller

        function onCurrentChanged()
        {
            Qt.callLater(root.resetWord)
        }
    }

    DictionaryController
    {
        id: dictionary
    }

    DesktopSessionBar
    {
        id: bar

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top

        title: qsTr("New words")
        muted: root.muted
        soundAvailable: root.speechOn
        showOptions: false
        opacity: screenMorph.barShown
        transform: Translate { y: -bar.height * (1 - screenMorph.barShown) }

        onExitRequested: root.leaveRequested()
        onSoundToggled: root.store.autoSpeak = !root.store.autoSpeak

        tools: [
            DesktopIconButton
            {
                objectName: "triage_range_toggle"

                visible: !!root.controller && !root.controller.deckMode && root.controller.maximumRank > 0
                iconName: "arrow_down"
                text: root.controller && root.controller.startRank > 0
                    ? qsTr("From #%1").arg(Qt.locale().toString(root.controller.startRank, "f", 0)) : qsTr("From the top")
                active: !!root.controller && root.controller.rangePaneOpen
                focusPolicy: Qt.NoFocus

                onClicked: root.controller.rangePaneOpen = !root.controller.rangePaneOpen
            },
            DesktopIconButton
            {
                objectName: "triage_added_toggle"

                iconName: "check"
                text: !root.controller ? qsTr("Added")
                    : root.controller.deckMode ? qsTr("Added %1").arg(root.controller.addedCount)
                    : qsTr("Added %1 / %2").arg(root.controller.addedCount).arg(root.controller.targetCount)
                active: !!root.controller && root.controller.addedListOpen
                focusPolicy: Qt.NoFocus

                onClicked: root.controller.addedListOpen = !root.controller.addedListOpen
            }
        ]
    }

    DesktopScreenMorph
    {
        id: screenMorph

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: bar.bottom
        anchors.bottom: parent.bottom
        landing: page
    }

    Item
    {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: bar.bottom
        anchors.bottom: parent.bottom
        opacity: screenMorph.contentShown

        Rectangle
        {
            id: page

            anchors.fill: parent
            color: AppTheme.colourPanel
        }

        DesktopSidePane
        {
            id: addedPane

            readonly property bool open: !!root.controller && root.controller.addedListOpen

            z: 1
            x: addedPane.open ? root.width - width : root.width
            anchors.top: parent.top
            anchors.bottom: actionBar.top
            widthKey: "triageAdded"
            defaultWidth: Math.round(root.width * AppMetrics.triagePaneShare)
            visible: x < root.width
            dividerOnLeft: true

            Behavior on x { NumberAnimation { duration: AppMetrics.motionBase; easing.type: AppMetrics.motionEasing } }

            DesktopSectionLabel
            {
                Layout.fillWidth: true
                Layout.leftMargin: AppMetrics.gapSmall
                Layout.topMargin: AppMetrics.gapSmall
                text: qsTr("Added")
            }

            ListView
            {
                id: addedList

                readonly property real rowWidth: width

                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: count > 0
                clip: true
                model: root.controller ? root.controller.addedWords : []
                boundsBehavior: Flickable.StopAtBounds

                Accessible.role: Accessible.List
                Accessible.name: qsTr("Added words")

                ScrollBar.vertical: DesktopScrollBar {}

                delegate: DesktopWordRow
                {
                    required property var modelData

                    word: modelData.word
                    translation: modelData.translation
                    partOfSpeech: modelData.partOfSpeech
                }
            }

            Item
            {
                Layout.fillHeight: true
                visible: addedList.count === 0
            }
        }

        DesktopSidePane
        {
            id: rangePane

            readonly property bool open: !!root.controller && !root.controller.deckMode && root.controller.rangePaneOpen
                                         && root.controller.maximumRank > 0

            z: 1
            x: rangePane.open ? 0 : -width
            anchors.top: parent.top
            anchors.bottom: actionBar.top
            width: rangeSlider.implicitWidth + AppMetrics.contentPadding * 2
            visible: x > -width

            Behavior on x { NumberAnimation { duration: AppMetrics.motionBase; easing.type: AppMetrics.motionEasing } }

            DesktopRankSlider
            {
                id: rangeSlider

                Layout.alignment: Qt.AlignHCenter
                Layout.fillHeight: true
                Layout.topMargin: AppMetrics.gapSmall
                Layout.bottomMargin: AppMetrics.gapSmall

                rank: root.controller ? root.controller.startRank : 0
                maximumRank: root.controller ? root.controller.maximumRank : 0
                levelMarks: root.controller ? root.controller.levelMarks : []

                onRankChosen: function(rank) { root.controller.startRank = rank }
            }
        }

        Item
        {
            id: detail

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: actionBar.top

            ColumnLayout
            {
                id: body

                x: (detail.width - width) / 2
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.topMargin: AppMetrics.contentPadding
                width: Math.min(AppMetrics.contentWidthNarrow, detail.width - AppMetrics.contentPadding * 2)
                spacing: AppMetrics.gapMedium

                DesktopEntryHeader
                {
                    id: entryHeader

                    centred: true
                    headwordSize: AppMetrics.triageWordSize
                    headword: root.controller ? root.controller.displayWord : ""
                    gender: root.controller ? root.controller.headwordGender : -1
                    meta: root.controller ? root.controller.cardMeta : ""
                    phonetic: root.entry ? root.entry.phonetic : ""
                    frequencyBucket: root.entry ? root.entry.frequencyBucket : ""
                    cefrLevel: root.entry ? root.entry.cefrLevel : ""

                    onSpeakRequested: root.controller.speakWord()
                    onWebSearchRequested: root.controller.openWebSearch()

                    actions: DesktopIconButton
                    {
                        objectName: "triage_dictionary"

                        iconName: "nav_words"
                        text: qsTr("Open in the dictionary")
                        enabled: root.hasWord

                        onClicked: root.dictionaryRequested(root.controller.word, root.controller.partOfSpeech)
                    }
                }

                DesktopTextField
                {
                    id: translationField

                    objectName: "triage_translation"

                    Layout.fillWidth: true
                    large: true
                    horizontalAlignment: TextInput.AlignHCenter
                    enabled: root.hasWord
                    text: root.hasMeanings ? root.meaningTranslation : (root.controller ? root.controller.translation : "")

                    onTextEdited:
                    {
                        if (root.hasMeanings)
                        {
                            root.meaningTranslation = text

                            return
                        }

                        if (root.controller && text !== root.controller.translation)
                        {
                            root.controller.translation = text
                        }
                    }

                    Keys.onReturnPressed: root.learn()
                    Keys.onEnterPressed: root.learn()
                }

                Flickable
                {
                    id: meaningArea

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    contentWidth: width
                    contentHeight: root.hasMeanings ? meaningColumn.implicitHeight : optionGrid.implicitHeight
                    boundsBehavior: Flickable.StopAtBounds
                    clip: true

                    ScrollBar.vertical: meaningScrollBar

                    ColumnLayout
                    {
                        id: meaningColumn

                        width: parent.width
                        visible: root.hasMeanings
                        spacing: AppMetrics.gapSmall

                        DesktopSectionLabel { text: qsTr("Meanings") }

                        Repeater
                        {
                            id: meaningRepeater

                            model: root.meanings

                            delegate: DesktopTriageMeaning
                            {
                                required property var modelData
                                required property int index

                                stageCount: root.store.stageCount
                                objectName: "triage_meaning_" + index

                                Layout.fillWidth: true
                                meaning: modelData
                                translations: modelData.translations.map(Text => root.translationRow(Text))
                                picked: root.pickedGist === modelData.gist ? root.pickedTranslations : []
                                expanded: index === root.openIndex
                                highlightedTranslation: expanded ? root.highlightedTranslation : -1

                                onExpandRequested: root.expandMeaning(index)
                                onWordClicked: function(word) { root.dictionaryRequested(word, "") }
                                onTranslationPicked: function(text) { root.pickTranslation(modelData.gist, text) }
                            }
                        }
                    }

                    Grid
                    {
                        id: optionGrid

                        readonly property real cellWidth: (width - columnSpacing * (columns - 1)) / columns

                        width: parent.width
                        visible: !root.hasMeanings
                        columns: 3
                        columnSpacing: AppMetrics.gapSmall
                        rowSpacing: AppMetrics.gapSmall

                        Repeater
                        {
                            model: root.hasMeanings ? [] : root.options

                            delegate: DesktopTranslationChip
                            {
                                required property var modelData
                                required property int index

                                width: optionGrid.cellWidth
                                text: modelData.text
                                relative: modelData.relative
                                hasProbability: modelData.hasProbability
                                learnedStage: modelData.learnedStage
                                inUse: modelData.selected
                                focusPolicy: Qt.NoFocus

                                ToolTip.text: inUse ? "" : qsTr("Use this translation")

                                onClicked: root.controller.toggleTranslation(index)
                            }
                        }
                    }

                    Rectangle
                    {
                        id: meaningFade

                        parent: meaningArea
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: AppMetrics.gapLarge * 2
                        visible: meaningArea.contentY + meaningArea.height < meaningArea.contentHeight - AppMetrics.borderWidth

                        gradient: Gradient
                        {
                            GradientStop { position: 0.0; color: AppTheme.faded(AppTheme.colourPanel, 0) }
                            GradientStop { position: 1.0; color: AppTheme.colourPanel }
                        }
                    }
                }
            }
        }

        DesktopScrollBar
        {
            id: meaningScrollBar

            parent: detail
            x: body.x + body.width + AppMetrics.gapSmall
            y: body.y + meaningArea.y
            height: meaningArea.height
        }

        DesktopActionBar
        {
            id: actionBar

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom

            DesktopButton
            {
                objectName: "triage_skip"

                Layout.preferredWidth: AppMetrics.actionButtonWidth
                large: true
                iconName: "arrow_left"
                text: qsTr("Skip")
                keyHint: qsTr("Backspace")
                enabled: root.hasWord
                focusPolicy: Qt.NoFocus

                onClicked: root.skip()
            }

            DesktopButton
            {
                objectName: "triage_learn"

                Layout.preferredWidth: AppMetrics.actionButtonWidth
                large: true
                tone: "primary"
                text: qsTr("Learn it")
                trailingIconName: "arrow_right"
                keyHint: qsTr("Enter")
                enabled: root.canLearn
                focusPolicy: Qt.NoFocus

                onClicked: root.learn()
            }
        }
    }

    Shortcut
    {
        sequences: ["Return", "Enter"]
        enabled: root.visible && root.canLearn
        context: Qt.WindowShortcut

        onActivated: root.learn()
    }

    Shortcut
    {
        sequence: "Backspace"
        enabled: root.visible && root.hasWord && !translationField.activeFocus
        context: Qt.WindowShortcut

        onActivated: root.skip()
    }

    Shortcut
    {
        sequence: "Ctrl+Space"
        enabled: root.visible && root.hasWord
        context: Qt.WindowShortcut

        onActivated: entryHeader.speak()
    }

    Shortcut
    {
        sequence: "Up"
        enabled: root.keysOnMeanings
        context: Qt.WindowShortcut

        onActivated: root.expandMeaning(root.openIndex - 1)
    }

    Shortcut
    {
        sequence: "Down"
        enabled: root.keysOnMeanings
        context: Qt.WindowShortcut

        onActivated: root.expandMeaning(root.openIndex + 1)
    }

    Shortcut
    {
        sequence: "Left"
        enabled: root.keysOnMeanings
        context: Qt.WindowShortcut

        onActivated: root.moveHighlight(-1)
    }

    Shortcut
    {
        sequence: "Right"
        enabled: root.keysOnMeanings
        context: Qt.WindowShortcut

        onActivated: root.moveHighlight(1)
    }

    Shortcut
    {
        sequence: "Space"
        enabled: root.keysOnMeanings
        context: Qt.WindowShortcut

        onActivated: root.pickHighlighted()
    }

    Shortcut
    {
        sequence: "Escape"
        enabled: root.visible
        context: Qt.WindowShortcut

        onActivated: root.leaveRequested()
    }
}
