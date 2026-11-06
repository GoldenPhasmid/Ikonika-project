import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

FocusScope
{
    id: root

    required property var store
    property var session: null
    property string sessionTitle: ""
    property int wordCount: 0
    property int elapsedSeconds: 0
    property string completeHeadline: qsTr("Session complete")
    property string countLabel: qsTr("words trained")

    property bool suspended: false

    signal leaveRequested()
    signal endRequested()
    signal finishedClicked()
    signal editCardRequested(var controller)
    signal addWordRequested(string word, string translation, string partOfSpeech, string hint)

    readonly property var active: root.session ? root.session.activeController : null
    readonly property bool finished: !!root.session && root.session.finished
    readonly property bool speechOn: !!root.store && root.store.audioEnabled
    readonly property bool muted: !root.speechOn || !root.store.autoSpeak
    readonly property bool sentenceMode: root.session instanceof SentenceTrainingController
    readonly property bool formsMode: !!root.session && !!root.session.formsSession
    readonly property bool multiStage: !!root.session && root.session.stageCount > 1
    readonly property var forms: root.active && root.kind !== "forms" ? root.active.forms : []

    property bool explainOpen: false

    readonly property DesktopScreenMorph morph: screenMorph
    readonly property Item cardSurface: exerciseLoader.item ? exerciseLoader.item.surface : null

    readonly property string kind: root.kindOf(root.active)

    function kindOf(controller)
    {
        if (!controller)
        {
            return ""
        }

        if (controller instanceof FlashcardController)
        {
            return root.formsMode ? "forms" : "flashcards"
        }

        if (controller instanceof FlipRecallController)
        {
            return "flip"
        }

        if (controller instanceof MultipleChoiceController)
        {
            return "choice"
        }

        if (controller instanceof WrittenProductionController || controller instanceof AudioTranscriptionController)
        {
            return "typed"
        }

        if (controller instanceof SpeedMatchController)
        {
            return "match"
        }

        if (controller instanceof CorrectIncorrectController)
        {
            return "review"
        }

        if (controller instanceof SentenceTilesController)
        {
            return "tiles"
        }

        return ""
    }

    function bindExercise()
    {
        const item = exerciseLoader.item

        if (item && item.kind === root.kindOf(root.active))
        {
            item.controller = root.active
        }
    }

    onActiveChanged: root.bindExercise()

    readonly property var components: ({
        "flashcards": flashcardComponent,
        "forms": formsComponent,
        "flip": flipComponent,
        "choice": choiceComponent,
        "typed": typedComponent,
        "match": matchComponent,
        "review": reviewComponent,
        "tiles": tilesComponent
    })

    function explainWord(word, sentence)
    {
        lookupPane.show(word, false, sentence)
        root.explainOpen = true
    }

    function openLookup()
    {
        lookupPane.show("", true)
        root.explainOpen = true
    }

    function toggleLookup()
    {
        if (root.explainOpen)
        {
            root.closeExplanation()

            return
        }

        root.openLookup()
    }

    function closeExplanation()
    {
        root.explainOpen = false
        Qt.callLater(root.focusExercise)
    }

    function askToLeave()
    {
        if (root.finished)
        {
            root.finishedClicked()

            return
        }

        root.leaveRequested()
    }

    readonly property bool lookupTyping: root.explainOpen && lookupPane.typing

    readonly property bool overlayOpen: root.suspended || optionsPopover.visible || formsPopover.visible
                                        || endDialog.visible

    readonly property var sessionKeys: [
        { sequence: "1", key: Qt.Key_1, text: "1" },
        { sequence: "2", key: Qt.Key_2, text: "2" },
        { sequence: "3", key: Qt.Key_3, text: "3" },
        { sequence: "4", key: Qt.Key_4, text: "4" },
        { sequence: "5", key: Qt.Key_5, text: "5" },
        { sequence: "6", key: Qt.Key_6, text: "6" },
        { sequence: "7", key: Qt.Key_7, text: "7" },
        { sequence: "8", key: Qt.Key_8, text: "8" },
        { sequence: "9", key: Qt.Key_9, text: "9" },
        { sequence: "0", key: Qt.Key_0, text: "0" },
        { sequence: "Q", key: Qt.Key_Q, text: "Q" },
        { sequence: "W", key: Qt.Key_W, text: "W" },
        { sequence: "E", key: Qt.Key_E, text: "E" },
        { sequence: "R", key: Qt.Key_R, text: "R" },
        { sequence: "T", key: Qt.Key_T, text: "T" },
        { sequence: "Y", key: Qt.Key_Y, text: "Y" },
        { sequence: "Return", key: Qt.Key_Return, text: "" },
        { sequence: "Enter", key: Qt.Key_Enter, text: "" },
        { sequence: "Space", key: Qt.Key_Space, text: " " },
        { sequence: "Left", key: Qt.Key_Left, text: "" },
        { sequence: "Right", key: Qt.Key_Right, text: "" },
        { sequence: "Down", key: Qt.Key_Down, text: "" }
    ]

    function runOnExercise(action, argument)
    {
        const item = exerciseLoader.item

        if (!item || !item[action])
        {
            return
        }

        const fromLookup = root.lookupTyping

        item[action](argument)

        if (fromLookup)
        {
            Qt.callLater(root.focusExercise)
        }
    }

    function dispatchKey(spec)
    {
        root.runOnExercise("handleKey", { key: spec.key, text: spec.text, modifiers: spec.modifiers || Qt.NoModifier })
    }

    function focusExercise()
    {
        exerciseHost.forceActiveFocus()

        if (exerciseLoader.item && exerciseLoader.item.focusAnswer)
        {
            exerciseLoader.item.focusAnswer()
        }
    }

    onVisibleChanged:
    {
        root.explainOpen = false

        if (root.visible)
        {
            Qt.callLater(root.focusExercise)
        }
    }

    DesktopSessionBar
    {
        id: bar

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top

        title: root.sessionTitle
        muted: root.muted
        soundAvailable: root.speechOn
        showOptions: !root.finished
        divided: false
        color: AppTheme.colourPage
        opacity: screenMorph.barShown
        transform: Translate { y: -bar.height * (1 - screenMorph.barShown) }

        onExitRequested: root.askToLeave()
        onSoundToggled: root.store.autoSpeak = !root.store.autoSpeak
        onOptionsRequested: optionsPopover.open()

        tools: [
            DesktopIconButton
            {
                id: formsButton

                objectName: "session_forms"

                visible: !root.finished && root.forms.length > 0
                enabled: exerciseLoader.item !== null && !exerciseLoader.item.formsHidden
                iconName: "info"
                active: formsPopover.visible
                text: qsTr("Show all forms")

                onClicked: formsPopover.visible ? formsPopover.close() : formsPopover.open()
            },
            DesktopIconButton
            {
                objectName: "session_lookup"

                visible: !root.finished
                iconName: "search"
                active: root.explainOpen
                text: qsTr("Look up a word (Ctrl+F)")

                onClicked: root.toggleLookup()
            },
            DesktopIconButton
            {
                objectName: "session_edit_card"

                visible: !root.finished && exerciseLoader.item !== null && !!exerciseLoader.item.editable
                         && !root.formsMode
                iconName: "edit"
                text: qsTr("Edit card")

                onClicked: root.editCardRequested(root.active)
            }
        ]
    }

    FocusScope
    {
        id: exerciseHost

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: bar.bottom
        anchors.bottom: parent.bottom

        focus: true
        visible: !root.finished

        Rectangle
        {
            anchors.fill: parent
            color: AppTheme.colourPage
            opacity: screenMorph.pageShown
        }

        DesktopScreenMorph
        {
            id: screenMorph

            anchors.fill: parent
            landing: root.finished ? null : root.cardSurface
        }

        Loader
        {
            id: exerciseLoader

            anchors.fill: parent
            opacity: screenMorph.contentShown
            sourceComponent: root.kind.length > 0 ? root.components[root.kind] : null

            onLoaded:
            {
                root.bindExercise()
                Qt.callLater(root.focusExercise)
            }
        }
    }

    DesktopSessionProgress
    {
        id: progress

        objectName: "session_progress"

        parent: bar
        anchors.centerIn: parent
        visible: !!root.session && !root.finished

        stage: root.session ? root.session.stage : 0
        stageCount: root.session ? root.session.stageCount : 0
        stageProgress: root.active ? root.active.exerciseProgress : 0
        opacity: screenMorph.contentShown
    }

    Rectangle
    {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: bar.bottom
        anchors.bottom: parent.bottom
        visible: root.finished
        color: AppTheme.colourPage

        DesktopSessionComplete
        {
            anchors.fill: parent
            visible: root.finished
            focus: root.finished

            headline: root.completeHeadline
            countLabel: root.countLabel
            wordCount: root.wordCount
            elapsedSeconds: root.elapsedSeconds

            onDoneClicked: root.finishedClicked()
        }
    }

    Repeater
    {
        model: root.sessionKeys

        delegate: Item
        {
            id: keyHolder

            required property var modelData

            readonly property bool commitsAnswer: modelData.key === Qt.Key_Return || modelData.key === Qt.Key_Enter

            Shortcut
            {
                sequence: keyHolder.modelData.sequence
                enabled: root.visible && !root.finished && !root.overlayOpen && (keyHolder.commitsAnswer || !root.lookupTyping)
                context: Qt.WindowShortcut

                onActivated: root.dispatchKey(keyHolder.modelData)
            }
        }
    }

    Shortcut
    {
        sequence: "Ctrl+D"
        enabled: root.visible && !root.finished && !root.overlayOpen
        context: Qt.WindowShortcut

        onActivated: root.runOnExercise("dontKnow")
    }

    Shortcut
    {
        sequence: "Ctrl+Space"
        enabled: root.visible && !root.finished && !root.overlayOpen
        context: Qt.WindowShortcut

        onActivated: root.runOnExercise("playVoice")
    }

    Shortcut
    {
        sequences: [StandardKey.Find]
        enabled: root.visible && !root.finished && !root.suspended
        context: Qt.WindowShortcut

        onActivated: root.toggleLookup()
    }

    Shortcut
    {
        sequences: ["Return", "Enter"]
        enabled: root.visible && root.finished && !root.overlayOpen
        context: Qt.WindowShortcut

        onActivated: root.finishedClicked()
    }

    Shortcut
    {
        sequence: "Escape"
        enabled: root.visible && !root.overlayOpen && !root.explainOpen
        context: Qt.WindowShortcut

        onActivated: root.askToLeave()
    }

    Shortcut
    {
        sequence: "Escape"
        enabled: root.visible && root.explainOpen && !root.suspended
        context: Qt.WindowShortcut

        onActivated: root.closeExplanation()
    }

    DictionaryController
    {
        id: lookup
    }

    DesktopLookupPane
    {
        id: lookupPane

        anchors.right: parent.right
        anchors.top: bar.bottom
        anchors.bottom: parent.bottom
        visible: root.explainOpen

        lookup: lookup
        maxStage: root.store ? root.store.stageCount : 1

        onAddWordRequested: function(word, translation, partOfSpeech, hint) { root.addWordRequested(word, translation, partOfSpeech, hint) }
    }

    Component
    {
        id: flashcardComponent

        DesktopFlashcardExercise
        {
            store: root.store
            sentenceText: root.sentenceMode

            onWordClicked: function(word, sentence) { root.explainWord(word, sentence) }
        }
    }

    Component { id: formsComponent; DesktopFormsStudyExercise { store: root.store } }
    Component { id: flipComponent; DesktopFlipExercise { store: root.store; onWordClicked: function(word, sentence) { root.explainWord(word, sentence) } } }
    Component { id: choiceComponent; DesktopChoiceExercise { store: root.store } }

    Component
    {
        id: typedComponent

        DesktopTypedExercise
        {
            store: root.store
            sentenceText: root.sentenceMode

            onWordClicked: function(word, sentence) { root.explainWord(word, sentence) }
        }
    }

    Component { id: matchComponent; DesktopMatchExercise { store: root.store } }
    Component { id: reviewComponent; DesktopReviewExercise { store: root.store; onWordClicked: function(word, sentence) { root.explainWord(word, sentence) } } }
    Component { id: tilesComponent; DesktopTilesExercise { store: root.store; onWordClicked: function(word, sentence) { root.explainWord(word, sentence) } } }

    DesktopPopover
    {
        id: optionsPopover

        parent: bar.optionsButton
        x: bar.optionsButton.width - width
        y: bar.optionsButton.height + AppMetrics.gapTight
        heading: qsTr("Session")

        onClosed: Qt.callLater(root.focusExercise)

        DesktopPopoverRow
        {
            visible: exerciseLoader.item !== null && exerciseLoader.item.voiceSupported
            text: qsTr("Play words aloud")
            toggle: true
            toggledOn: exerciseLoader.item !== null && exerciseLoader.item.voiceOn && root.speechOn
            enabled: root.speechOn

            onClicked: exerciseLoader.item.toggleVoice()
        }

        DesktopPopoverRow
        {
            objectName: "session_sentence_typing"

            visible: exerciseLoader.item !== null && !!exerciseLoader.item.typingSupported
            text: qsTr("Hard mode")
            toggle: true
            toggledOn: exerciseLoader.item !== null && !!exerciseLoader.item.typingOn

            onClicked: exerciseLoader.item.toggleTyping()
        }

        DesktopPopoverRow
        {
            objectName: "session_restart_stage"

            text: qsTr("Restart this stage")
            iconName: "undo"

            onClicked:
            {
                optionsPopover.close()
                root.session.restartStage()
            }
        }

        DesktopPopoverRow
        {
            objectName: "session_complete_stage"

            visible: root.multiStage
            text: qsTr("Skip to the next stage")
            iconName: "arrow_right"

            onClicked:
            {
                optionsPopover.close()
                root.session.completeStage()
            }
        }

        DesktopDivider
        {
            Layout.topMargin: AppMetrics.gapTight
            Layout.bottomMargin: AppMetrics.gapTight
        }

        DesktopPopoverRow
        {
            objectName: "session_end"

            text: qsTr("End session")
            iconName: "trash"
            destructive: true

            onClicked:
            {
                optionsPopover.close()
                endDialog.open()
            }
        }
    }

    DesktopPopover
    {
        id: formsPopover

        parent: formsButton
        x: formsButton.width - width
        y: formsButton.height + AppMetrics.gapTight
        width: AppMetrics.dialogWidth
        heading: root.active && root.active.tense.length > 0 ? root.active.tense : qsTr("Forms")

        onClosed: Qt.callLater(root.focusExercise)

        DesktopFormsGrid
        {
            Layout.leftMargin: AppMetrics.gapCompact
            Layout.rightMargin: AppMetrics.gapCompact
            Layout.bottomMargin: AppMetrics.gapCompact
            forms: root.forms
        }
    }

    DesktopConfirmDialog
    {
        id: endDialog

        title: qsTr("End this session?")
        message: qsTr("Progress in this session is lost and its words go back to the pool.")
        confirmLabel: qsTr("End session")
        action: function() { root.endRequested() }

        onClosed: Qt.callLater(root.focusExercise)
    }
}
