import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item
{
    id: root

    property var controller: null
    property var store: null

    readonly property string kind: "match"
    readonly property Item surface: card.surface
    readonly property bool editable: false

    readonly property string title: qsTr("Match the pairs")
    readonly property bool voiceSupported: !!root.controller && root.controller.voiceSupported
    readonly property bool voiceOn: !!root.controller && root.controller.voiceRequested

    readonly property int rowCount: root.controller ? Math.min(root.controller.leftItems.length, root.controller.rightItems.length) : 0

    readonly property bool styledRight: !!root.controller && root.controller.rightMarkups.length === root.controller.rightItems.length
                                        && root.controller.rightMarkups.length > 0

    readonly property bool styledLeft: root.styledRight && root.controller.leftMarkups.length === root.controller.leftItems.length

    readonly property string heading: root.controller ? root.controller.heading : ""
    readonly property bool sixRows: root.rowCount > 5

    readonly property var leftKeys: root.sixRows ? ["1", "2", "3", "4", "5", "6"] : ["1", "2", "3", "4", "5"]
    readonly property var rightKeys: root.sixRows ? ["Q", "W", "E", "R", "T", "Y"] : ["6", "7", "8", "9", "0"]

    function playVoice()
    {
        if (headingLabel.visible)
        {
            headingLabel.speak()
        }
    }

    function toggleVoice()
    {
        if (root.controller)
        {
            root.controller.sessionSettings.matchVoice = !root.controller.sessionSettings.matchVoice
        }
    }

    function escaped(text)
    {
        return text.replace(/&/g, "&amp;").replace(/</g, "&lt;")
    }

    readonly property var cells:
    {
        const flat = []

        for (let i = 0; i < root.rowCount; i++)
        {
            flat.push(root.styledLeft ? root.controller.leftMarkups[i]
                : (root.styledRight ? root.escaped(root.controller.leftItems[i]) : root.controller.leftItems[i]))
            flat.push(root.styledRight ? root.controller.rightMarkups[i] : root.controller.rightItems[i])
        }

        return flat
    }

    readonly property var cellHints:
    {
        const hints = []

        for (let i = 0; i < root.rowCount; i++)
        {
            hints.push(i < root.leftKeys.length ? root.leftKeys[i] : "")
            hints.push(i < root.rightKeys.length ? root.rightKeys[i] : "")
        }

        return hints
    }

    readonly property var cellColours:
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
            const row = Math.floor(i / 2)
            const matched = i % 2 === 0 ? root.controller.leftMatched[row] : root.controller.rightMatched[row]

            fades.push(matched === true ? 0.5 : 1.0)
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

    function handleKey(event)
    {
        if (!root.controller || !root.controller.awaitingAnswer)
        {
            return false
        }

        const typed = event.text.toUpperCase()

        const left = root.leftKeys.indexOf(typed)

        if (left >= 0 && left < root.rowCount)
        {
            root.controller.tapLeft(left)

            return true
        }

        const right = root.rightKeys.indexOf(typed)

        if (right >= 0 && right < root.rowCount)
        {
            root.controller.tapRight(right)

            return true
        }

        return false
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
            if (root.controller.showingFeedback && !advanceDelay.running)
            {
                advanceDelay.start()
            }
        }
    }

    DesktopExerciseCard
    {
        id: card

        anchors.fill: parent

        primarySpeaks: headingLabel.visible
        onPrimaryClicked: root.playVoice()
        tags: root.controller ? [root.controller.tense] : []
        counterText: root.controller ? qsTr("%1 / %2").arg(root.controller.currentSessionCount).arg(root.controller.maxSessionCount) : ""
        showPrimary: root.heading.length > 0
        compactPrompt: true
        centreSecondary: true

        primary: DesktopPromptLabel
        {
            id: headingLabel

            visible: root.heading.length > 0
            text: root.heading
            maximumSize: AppMetrics.fontHeadline
            speakable: true

            Accessible.role: Accessible.Heading
            Accessible.name: text

            onSpeakRequested: root.controller.speakHeading()
        }

        secondary: DesktopOptionGrid
        {
            id: matchGrid

            Layout.fillWidth: true

            cardModel: root.cells
            verbGroups: root.controller ? root.controller.leftVerbGroups.flatMap(group => [group, ""]) : []
            cellHeight: root.heading.length > 0 ? AppMetrics.matchCellHeightCompact : AppMetrics.matchCellHeight
            rowSpacing: root.heading.length > 0 ? AppMetrics.gapSmall : AppMetrics.gapCompact
            keyHints: root.cellHints
            styledText: root.styledRight
            voiceColumn: !!root.controller && root.controller.voiceLeft
            playingIndex: root.controller && root.controller.speaking ? root.controller.selectedLeftRow * 2 : -1
            interactive: !!root.controller && root.controller.awaitingAnswer
            stateColours: root.cellColours
            cellOpacities: root.cellFades

            onOptionClicked: function(index) { root.tapCell(index) }
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
}
