import QtQuick

Item
{
    id: root

    property var store: null

    property alias controller: controller

    readonly property bool hasActiveSession: controller.hasActiveSession

    signal done()
    signal editCardRequested(var exercise)
    signal lookupRequested(string word, string sentence)

    IntensiveLearningController
    {
        id: controller
    }

    DailySessionController
    {
        id: session
    }

    function resume(sessionKey)
    {
        return controller.resume(sessionKey)
    }

    function start()
    {
        if (controller.resume(controller.vocabularyKey))
        {
            return true
        }

        return controller.startCards(controller.vocabularyKey, session.collectSessionCards())
    }

    function startWithCards(sessionKey, cardIds)
    {
        if (controller.resume(sessionKey))
        {
            return true
        }

        return controller.startCards(sessionKey, cardIds)
    }

    function endActiveSession()
    {
        controller.endSession()
        root.done()
    }

    function bindStage(item)
    {
        item.showComplete = false
        item.inSession = true
        item.done.connect(root.done)
        item.endSessionRequested.connect(root.endActiveSession)
        item.restartRequested.connect(controller.restartStage)
        item.completeRequested.connect(controller.completeStage)

        if (item.wordTapped !== undefined)
        {
            item.wordTapped.connect(root.lookupRequested)
        }

        if (item.cardEditable === undefined)
        {
            return
        }

        item.cardEditable = Qt.binding(function() { return !controller.formsSession })
        item.editCardRequested.connect(function() { root.editCardRequested(item.controller) })
    }

    readonly property bool onStudy: controller.activeController instanceof FlashcardController
    readonly property bool onFlip: controller.activeController instanceof FlipRecallController
    readonly property bool onChoice: controller.activeController instanceof MultipleChoiceController
    readonly property bool onProduction: controller.activeController instanceof AudioTranscriptionController
        || controller.activeController instanceof WrittenProductionController

    Loader
    {
        id: flashcardLoader

        anchors.fill: parent
        active: !controller.formsSession && root.onStudy
        visible: active && !controller.finished
        sourceComponent: flashcardComponent

        Binding
        {
            target: flashcardLoader.item
            property: "controller"
            value: controller.activeController
            when: flashcardLoader.item && controller.activeController instanceof FlashcardController
            restoreMode: Binding.RestoreNone
            delayed: true
        }

        onLoaded: root.bindStage(item)
    }

    Loader
    {
        id: formsStudyLoader

        anchors.fill: parent
        active: controller.formsSession && root.onStudy
        visible: active && !controller.finished
        sourceComponent: formsStudyComponent

        Binding
        {
            target: formsStudyLoader.item
            property: "controller"
            value: controller.activeController
            when: formsStudyLoader.item && controller.activeController instanceof FlashcardController
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
        visible: active && !controller.finished
        sourceComponent: flipComponent

        Binding
        {
            target: flipLoader.item
            property: "controller"
            value: controller.activeController
            when: flipLoader.item && controller.activeController instanceof FlipRecallController
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
        visible: active && !controller.finished
        sourceComponent: choiceComponent

        Binding
        {
            target: choiceLoader.item
            property: "controller"
            value: controller.activeController
            when: choiceLoader.item && controller.activeController instanceof MultipleChoiceController
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
        visible: active && !controller.finished
        sourceComponent: controller.activeController instanceof AudioTranscriptionController
            ? audioComponent : writtenComponent

        Binding
        {
            target: productionLoader.item
            property: "controller"
            value: controller.activeController
            when: productionLoader.item && (controller.activeController instanceof AudioTranscriptionController) === (productionLoader.sourceComponent === audioComponent)
            restoreMode: Binding.RestoreNone
            delayed: true
        }

        onLoaded:
        {
            root.bindStage(item)

            function refocusIfStillTyping()
            {
                if (root.onProduction && !controller.finished)
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
        active: controller.activeController instanceof SpeedMatchController
        visible: active && !controller.finished
        sourceComponent: matchComponent

        Binding
        {
            target: matchLoader.item
            property: "controller"
            value: controller.activeController
            when: matchLoader.item && controller.activeController instanceof SpeedMatchController
            restoreMode: Binding.RestoreNone
            delayed: true
        }

        onLoaded: root.bindStage(item)
    }

    Loader
    {
        id: tilesLoader

        anchors.fill: parent
        active: controller.activeController instanceof SentenceTilesController
        visible: active && !controller.finished

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
        visible: controller.finished

        exerciseName: qsTr("Training")

        wordCount: controller.sessionWordCount
        countLabel: qsTr("words trained")
        elapsedSeconds: controller.elapsedSeconds

        onDoneClicked: root.done()
    }
}
