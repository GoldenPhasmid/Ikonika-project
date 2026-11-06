import QtQuick
import QtQuick.Layouts

Item
{
    id: root

    property var controller: SpeedMatchController {}

    property var store: null

    property bool showComplete: true
    property bool inSession: false
    property bool manualAdvance: false

    readonly property bool finished: root.controller.finished

    signal done()
    signal endSessionRequested()
    signal restartRequested()
    signal completeRequested()

    readonly property int rowCount:
        Math.min(root.controller.leftItems.length, root.controller.rightItems.length)

    readonly property bool styledRight: root.controller.rightMarkups.length === root.controller.rightItems.length
                                        && root.controller.rightMarkups.length > 0

    readonly property bool styledLeft: root.styledRight
                                       && root.controller.leftMarkups.length === root.controller.leftItems.length

    readonly property var cells:
    {
        const flat = []

        for (let i = 0; i < root.rowCount; i++)
        {
            flat.push(root.styledLeft ? root.controller.leftMarkups[i]
                : root.styledRight ? root.controller.leftItems[i].replace(/&/g, "&amp;").replace(/</g, "&lt;")
                                   : root.controller.leftItems[i])
            flat.push(root.styledRight ? root.controller.rightMarkups[i] : root.controller.rightItems[i])
        }

        return flat
    }

    readonly property var cellBorders:
    {
        const colours = []

        for (let i = 0; i < root.cells.length; i++)
        {
            const onLeft = i % 2 === 0
            const row = Math.floor(i / 2)

            if (row === (onLeft ? root.controller.flashedLeftRow : root.controller.flashedRightRow))
            {
                colours.push(AppTheme.colourWrong)
                continue
            }

            if ((onLeft ? root.controller.leftMatched[row] : root.controller.rightMatched[row]) === true)
            {
                colours.push(AppTheme.colourCorrect)
                continue
            }

            if (row === (onLeft ? root.controller.selectedLeftRow : root.controller.selectedRightRow))
            {
                colours.push(AppTheme.colourAccent)
                continue
            }

            colours.push(AppTheme.colourBorder)
        }

        return colours
    }

    readonly property var cellFades:
    {
        const fades = []

        for (let i = 0; i < root.cells.length; i++)
        {
            const onLeft = i % 2 === 0
            const row = Math.floor(i / 2)
            const matched = onLeft
                ? root.controller.leftMatched[row]
                : root.controller.rightMatched[row]

            fades.push(matched === true ? 0.7 : 1.0)
        }

        return fades
    }

    function tapCell(index)
    {
        const row = Math.floor(index / 2)

        if (index % 2 === 0)
        {
            root.controller.tapLeft(row)

            return
        }

        root.controller.tapRight(row)
    }

    Timer
    {
        id: advanceDelay

        interval: AppMetrics.exerciseAdvanceDelayCorrect
        onTriggered: root.controller.advance()
    }

    Connections
    {
        target: root.controller

        function onAnswerGraded(correct)
        {
            matchGrid.armFeedback(correct)
        }

        function onStateChanged()
        {
            if (!root.manualAdvance && root.controller.showingFeedback && !advanceDelay.running)
            {
                advanceDelay.start()
            }
        }
    }

    MobileExerciseShell
    {
        id: shell

        anchors.fill: parent
        visible: !(root.finished && root.showComplete)

        exerciseName: qsTr("Match a pair")
        store: root.store

        progress: root.controller.exerciseProgress
        hasActions: root.manualAdvance

        questionSideNames: root.store ? root.store.questionSideNames : []
        questionSideIndex: root.store ? root.store.questionSide : 0
        inSession: root.inSession

        onExitRequested: root.done()
        onEndSessionRequested: root.endSessionRequested()

        showVoiceOption: root.store !== null && root.controller.voiceSupported
        voiceOn: root.controller.voiceRequested
        onVoiceToggled: root.controller.sessionSettings.matchVoice = !root.controller.sessionSettings.matchVoice

        onRestartRequested:
        {
            if (root.inSession)
            {
                root.restartRequested()
            }
            else
            {
                root.controller.restart()
            }
        }

        onCompleteRequested: root.completeRequested()
        onQuestionSideChosen: function(index) { if (root.store) root.store.questionSide = index }

        actions: MobileSessionButton
        {
            visible: root.manualAdvance
            label: qsTr("Continue")
            interactive: root.controller.showingFeedback

            onClicked: root.controller.advance()
        }

        MobileContentArea
        {
            id: contentArea

            posText: root.controller.tense
            posVisible: root.controller.tense.length > 0
            showPrimary: false
            reserveActionsSpace: !root.manualAdvance
            counterText: qsTr("%1 / %2")
                .arg(root.controller.currentSessionCount)
                .arg(root.controller.maxSessionCount)

            secondaryContent: MobileOptionGrid
            {
                id: matchGrid

                Layout.fillWidth: true

                cardModel: root.cells
                verbGroups: root.controller ? root.controller.leftVerbGroups.flatMap(group => [group, ""]) : []
                fitHeight: contentArea.secondaryHeight
                styledText: root.styledRight
                interactive: root.controller ? root.controller.awaitingAnswer : false
                voiceColumn: root.controller.voiceLeft
                playingIndex: root.controller.speaking ? root.controller.selectedLeftRow * 2 : -1

                borderColours: root.cellBorders
                cellOpacities: root.cellFades

                opacity: root.controller.showingFeedback && !root.manualAdvance ? 0.0 : 1.0

                Behavior on opacity { NumberAnimation { duration: AppMetrics.motionSlow } }

                onOptionClicked: function(index) { root.tapCell(index) }
            }
        }
    }

    MouseArea
    {
        anchors.fill: parent
        enabled: advanceDelay.running

        onClicked:
        {
            advanceDelay.stop()
            root.controller.advance()
        }
    }

    MobileExerciseCompleteScreen
    {
        anchors.fill: parent
        visible: root.finished && root.showComplete

        exerciseName: qsTr("Match a pair")

        wordCount: root.controller.currentSessionCount
        elapsedSeconds: root.controller.elapsedSeconds

        onDoneClicked: root.done()
    }
}
