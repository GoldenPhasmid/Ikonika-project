import QtQuick

Item
{
    id: root

    property var store: null

    required property RepetitionController controller

    readonly property bool hasActiveSession: root.controller.hasActiveSession

    signal done()
    signal editCardRequested(var exercise)
    signal lookupRequested(string word, string sentence, var place)

    function startDueToday()
    {
        return root.controller.startDueToday()
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

        item.cardEditable = true
        item.editCardRequested.connect(function() { root.editCardRequested(item.controller) })
    }

    Loader
    {
        id: reviewLoader

        anchors.fill: parent
        active: root.controller.activeController instanceof CorrectIncorrectController
        visible: active && !root.controller.finished
        sourceComponent: reviewComponent

        Binding
        {
            target: reviewLoader.item
            property: "controller"
            value: root.controller.activeController
            when: reviewLoader.item && root.controller.activeController instanceof CorrectIncorrectController
            restoreMode: Binding.RestoreNone
            delayed: true
        }

        onLoaded: root.bindStage(item)
    }

    readonly property bool onChoice: root.controller.activeController instanceof MultipleChoiceController
    readonly property bool onProduction: root.controller.activeController instanceof AudioTranscriptionController
        || root.controller.activeController instanceof WrittenProductionController
    readonly property bool onMatch: root.controller.activeController instanceof SpeedMatchController

    Loader
    {
        id: choiceLoader

        anchors.fill: parent
        active: root.onChoice
        visible: root.onChoice && !root.controller.finished
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
        visible: root.onProduction && !root.controller.finished
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
        active: root.onMatch
        visible: root.onMatch && !root.controller.finished
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

    Component { id: reviewComponent; MobileFlipScreen { store: root.store } }
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
        visible: root.controller.finished

        exerciseName: qsTr("Review")

        wordCount: root.controller.cardCount
        countLabel: qsTr("words trained")
        elapsedSeconds: root.controller.elapsedSeconds
        doneText: root.controller.followUpPending ? qsTr("Continue") : qsTr("Done")

        onDoneClicked:
        {
            if (root.controller.followUpPending)
            {
                root.controller.continueToFollowUp()

                return
            }

            root.done()
        }
    }
}
