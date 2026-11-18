import QtQuick

Item
{
    id: root

    property var store: null

    required property IntensiveLearningController controller
    required property DailySessionController session

    readonly property bool hasActiveSession: root.controller.hasActiveSession

    signal done()
    signal editCardRequested(var exercise)
    signal lookupRequested(string word, string sentence)

    function resume(sessionKey)
    {
        return root.controller.resume(sessionKey)
    }

    function start()
    {
        if (root.controller.resume(root.controller.vocabularyKey))
        {
            return true
        }

        return root.controller.startCards(root.controller.vocabularyKey, root.session.collectSessionCards())
    }

    function startWithCards(sessionKey, cardIds)
    {
        if (root.controller.resume(sessionKey))
        {
            return true
        }

        return root.controller.startCards(sessionKey, cardIds)
    }

    function endActiveSession()
    {
        root.controller.endSession()
        root.done()
    }

    function bindStage(item)
    {
        item.showComplete = false
        item.inSession = true
        item.done.connect(root.done)
        item.endSessionRequested.connect(root.endActiveSession)
        item.restartRequested.connect(root.controller.restartStage)
        item.completeRequested.connect(root.controller.completeStage)

        if (item.wordTapped !== undefined)
        {
            item.wordTapped.connect(root.lookupRequested)
        }

        if (item.cardEditable === undefined)
        {
            return
        }

        item.cardEditable = Qt.binding(function() { return !root.controller.formsSession })
        item.editCardRequested.connect(function() { root.editCardRequested(item.controller) })
    }

    readonly property bool onStudy: root.controller.activeController instanceof FlashcardController
    readonly property bool onFlip: root.controller.activeController instanceof FlipRecallController
    readonly property bool onChoice: root.controller.activeController instanceof MultipleChoiceController
    readonly property bool onProduction: root.controller.activeController instanceof AudioTranscriptionController
        || root.controller.activeController instanceof WrittenProductionController

    Loader
    {
        id: flashcardLoader

        anchors.fill: parent
        active: !root.controller.formsSession && root.onStudy
        visible: active && !root.controller.finished
        sourceComponent: flashcardComponent

        Binding
        {
            target: flashcardLoader.item
            property: "controller"
            value: root.controller.activeController
            when: flashcardLoader.item && root.controller.activeController instanceof FlashcardController
            restoreMode: Binding.RestoreNone
            delayed: true
        }

        onLoaded: root.bindStage(item)
    }

    Loader
    {
        id: formsStudyLoader

        anchors.fill: parent
        active: root.controller.formsSession && root.onStudy
        visible: active && !root.controller.finished
        sourceComponent: formsStudyComponent

        Binding
        {
            target: formsStudyLoader.item
            property: "controller"
            value: root.controller.activeController
            when: formsStudyLoader.item && root.controller.activeController instanceof FlashcardController
            restoreMode: Binding.RestoreNone
            delayed: true
        }

        onLoaded: root.bindStage(item)
    }

    Loader
    {
        id: flipLoader

        anchors.fill: parent
        active: root.onFlip
        visible: active && !root.controller.finished
        sourceComponent: flipComponent

        Binding
        {
            target: flipLoader.item
            property: "controller"
            value: root.controller.activeController
            when: flipLoader.item && root.controller.activeController instanceof FlipRecallController
            restoreMode: Binding.RestoreNone
            delayed: true
        }

        onLoaded: root.bindStage(item)
    }

    Loader
    {
        id: choiceLoader

        anchors.fill: parent
        active: root.onChoice
        visible: active && !root.controller.finished
        sourceComponent: choiceComponent

        Binding
        {
            target: choiceLoader.item
            property: "controller"
            value: root.controller.activeController
            when: choiceLoader.item && root.controller.activeController instanceof MultipleChoiceController
            restoreMode: Binding.RestoreNone
            delayed: true
        }

        onLoaded: root.bindStage(item)
    }

    Loader
    {
        id: productionLoader

        anchors.fill: parent
        active: root.onProduction
        visible: active && !root.controller.finished
        sourceComponent: root.controller.activeController instanceof AudioTranscriptionController
            ? audioComponent : writtenComponent

        Binding
        {
            target: productionLoader.item
            property: "controller"
            value: root.controller.activeController
            when: productionLoader.item && (root.controller.activeController instanceof AudioTranscriptionController) === (productionLoader.sourceComponent === audioComponent)
            restoreMode: Binding.RestoreNone
            delayed: true
        }

        onLoaded:
        {
            root.bindStage(item)

            function refocusIfStillTyping()
            {
                if (root.onProduction && !root.controller.finished)
                {
                    item.focusAnswer()
                }
            }

            item.controllerChanged.connect(refocusIfStillTyping)
            Qt.callLater(refocusIfStillTyping)
        }
    }

    Loader
    {
        id: matchLoader

        anchors.fill: parent
        active: root.controller.activeController instanceof SpeedMatchController
        visible: active && !root.controller.finished
        sourceComponent: matchComponent

        Binding
        {
            target: matchLoader.item
            property: "controller"
            value: root.controller.activeController
            when: matchLoader.item && root.controller.activeController instanceof SpeedMatchController
            restoreMode: Binding.RestoreNone
            delayed: true
        }

        onLoaded: root.bindStage(item)
    }

    Loader
    {
        id: tilesLoader

        anchors.fill: parent
        active: root.controller.activeController instanceof SentenceTilesController
        visible: active && !root.controller.finished

        sourceComponent: MobileSentenceTilesScreen
        {
            controller: root.controller.activeController
            store: root.store
        }

        onLoaded: root.bindStage(item)
    }

    Component { id: flashcardComponent; MobileFlashcardScreen { store: root.store } }
    Component { id: formsStudyComponent; MobileFormsStudyScreen { store: root.store } }
    Component { id: flipComponent; MobileFlipScreen { store: root.store } }
    Component { id: choiceComponent; MobileMultipleChoiceScreen { store: root.store } }
    Component
    {
        id: writtenComponent

        MobileWrittenProductionScreen
        {
            store: root.store
            manualAdvance: root.controller.formsSession
            exerciseName: qsTr("Compile word")
        }
    }
    Component { id: audioComponent; MobileAudioTranscriptionScreen { store: root.store } }
    Component { id: matchComponent; MobileSpeedMatchScreen { store: root.store } }

    MobileExerciseCompleteScreen
    {
        anchors.fill: parent
        visible: root.controller.finished

        exerciseName: qsTr("Training")

        wordCount: root.controller.sessionWordCount
        countLabel: qsTr("words trained")
        elapsedSeconds: root.controller.elapsedSeconds

        onDoneClicked: root.done()
    }
}
