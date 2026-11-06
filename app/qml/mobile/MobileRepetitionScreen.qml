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

    function startDueToday()
    {
        return controller.startDueToday()
    }

    function endActiveSession()
    {
        controller.endSession()
        root.done()
    }

    RepetitionController
    {
        id: controller
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

        item.cardEditable = true
        item.editCardRequested.connect(function() { root.editCardRequested(item.controller) })
    }

    Loader
    {
        id: reviewLoader

        anchors.fill: parent
        active: controller.activeController instanceof CorrectIncorrectController
        visible: active && !controller.finished
        sourceComponent: reviewComponent

        Binding
        {
            target: reviewLoader.item
            property: "controller"
            value: controller.activeController
            when: reviewLoader.item && controller.activeController instanceof CorrectIncorrectController
            restoreMode: Binding.RestoreNone
            delayed: true
        }

        onLoaded: root.bindStage(item)
    }

    readonly property bool onChoice: controller.activeController instanceof MultipleChoiceController
    readonly property bool onProduction: controller.activeController instanceof AudioTranscriptionController
        || controller.activeController instanceof WrittenProductionController
    readonly property bool onMatch: controller.activeController instanceof SpeedMatchController

    Loader
    {
        id: choiceLoader

        anchors.fill: parent
        active: root.onChoice
        visible: root.onChoice && !controller.finished
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
        visible: root.onProduction && !controller.finished
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
        active: root.onMatch
        visible: root.onMatch && !controller.finished
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

    Component { id: reviewComponent; MobileCorrectIncorrectScreen { store: root.store } }
    Component { id: choiceComponent; MobileMultipleChoiceScreen { store: root.store } }
    Component
    {
        id: writtenComponent

        MobileWrittenProductionScreen
        {
            store: root.store
            exerciseName: qsTr("Compile word")
        }
    }
    Component { id: audioComponent; MobileAudioTranscriptionScreen { store: root.store } }
    Component { id: matchComponent; MobileSpeedMatchScreen { store: root.store } }

    MobileExerciseCompleteScreen
    {
        anchors.fill: parent
        visible: controller.finished

        exerciseName: qsTr("Review")

        wordCount: controller.cardCount
        countLabel: qsTr("words trained")
        elapsedSeconds: controller.elapsedSeconds

        onDoneClicked: root.done()
    }
}
