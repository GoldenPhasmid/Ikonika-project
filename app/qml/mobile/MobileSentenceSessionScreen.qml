import QtQuick

Item
{
    id: root

    required property var controller

    property var store: null

    property bool explainOpen: false
    property bool explainPending: false
    property string explainedWord: ""

    readonly property var exercise: root.controller.activeController

    signal done()
    signal lookupRequested(string word, string sentence)

    function endActiveSession()
    {
        root.controller.endSession()
        root.done()
    }

    function explainWord(word)
    {
        Qt.inputMethod.hide()

        root.explainedWord = word
        root.explainPending = true
        explainLookup.search(word)
    }

    function closeExplanation()
    {
        root.explainOpen = false

        gapStage.focusAnswer()
        dictationStage.focusAnswer()
    }

    onVisibleChanged:
    {
        if (!root.visible)
        {
            root.explainPending = false
            root.explainOpen = false
        }
    }

    DictionaryController
    {
        id: explainLookup

        onResultsChanged:
        {
            if (root.explainPending)
            {
                root.explainPending = false
                root.explainOpen = true
            }
        }
    }

    function connectStage(item)
    {
        item.showComplete = false
        item.store = root.store
        item.inSession = true
        item.done.connect(root.done)
        item.endSessionRequested.connect(root.endActiveSession)
        item.restartRequested.connect(root.controller.restartStage)
        item.completeRequested.connect(root.controller.completeStage)
        item.wordHeld.connect(root.explainWord)

        if (item.wordTapped !== undefined)
        {
            item.wordTapped.connect(root.lookupRequested)
        }
    }

    Loader
    {
        id: flashcardStage

        anchors.fill: parent
        active: root.exercise instanceof FlashcardController
        visible: active && !root.controller.finished

        sourceComponent: MobileFlashcardScreen
        {
            sentenceText: true
        }

        Binding
        {
            target: flashcardStage.item
            property: "controller"
            value: root.exercise
            when: flashcardStage.item && root.exercise instanceof FlashcardController
            restoreMode: Binding.RestoreNone
        }

        onLoaded:
        {
            root.connectStage(item)
        }
    }

    Loader
    {
        anchors.fill: parent
        active: root.exercise instanceof SentenceTilesController
        visible: active && !root.controller.finished

        sourceComponent: MobileSentenceTilesScreen
        {
            controller: root.exercise
        }

        onLoaded: root.connectStage(item)
    }

    Loader
    {
        id: gapStage

        anchors.fill: parent
        active: root.exercise instanceof WrittenProductionController
        visible: active && !root.controller.finished

        sourceComponent: MobileWrittenProductionScreen
        {
            exerciseName: qsTr("Missing word")
            sentenceText: true
            manualAdvance: true
        }

        Binding
        {
            target: gapStage.item
            property: "controller"
            value: root.exercise
            when: gapStage.item && root.exercise instanceof WrittenProductionController
            restoreMode: Binding.RestoreNone
        }

        onLoaded:
        {
            root.connectStage(item)

            Qt.callLater(gapStage.focusAnswer)
        }

        function focusAnswer()
        {
            if (gapStage.item && gapStage.visible)
            {
                gapStage.item.focusAnswer()
            }
        }
    }

    Loader
    {
        id: dictationStage

        anchors.fill: parent
        active: root.exercise instanceof AudioTranscriptionController
        visible: active && !root.controller.finished

        sourceComponent: MobileAudioTranscriptionScreen
        {
            exerciseName: qsTr("Dictation")
            sentenceText: true
            manualAdvance: true
        }

        Binding
        {
            target: dictationStage.item
            property: "controller"
            value: root.exercise
            when: dictationStage.item && root.exercise instanceof AudioTranscriptionController
            restoreMode: Binding.RestoreNone
        }

        onLoaded:
        {
            root.connectStage(item)

            Qt.callLater(dictationStage.focusAnswer)
        }

        function focusAnswer()
        {
            if (dictationStage.item && dictationStage.visible)
            {
                dictationStage.item.focusAnswer()
            }
        }
    }

    MobileExerciseCompleteScreen
    {
        anchors.fill: parent
        visible: root.controller.finished

        exerciseName: qsTr("Sentences")

        wordCount: root.controller.sessionSentenceCount
        countLabel: qsTr("sentences practiced")
        elapsedSeconds: root.controller.elapsedSeconds

        onDoneClicked: root.endActiveSession()
    }

    MobileSheetLoader
    {
        anchors.fill: parent
        z: 20
        open: root.explainOpen

        sourceComponent: MobileWordExplainSheet
        {
            lookup: explainLookup
            word: root.explainedWord
            maxStage: root.store ? root.store.stageCount : 0

            onDismissed: root.closeExplanation()
        }
    }
}
