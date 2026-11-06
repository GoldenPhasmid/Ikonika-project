import QtQuick
import QtQuick.Layouts
import QtQuick.Window
import "../Greeting.js" as Greeting

FocusScope
{
    id: root

    property string name: ""
    property int reviewCount: 0
    property int newCount: 0
    property real reviewProgress: 0
    property real learnProgress: 0
    property int doneToday: 0
    property int dailyGoal: 0
    property bool showReviewRing: true
    property bool showLearnRing: true
    property Item target: null
    property bool shown: true

    signal finished()

    readonly property int holdDuration: 1500
    readonly property string greeting: Greeting.forHour(new Date().getHours(), root.name)
    readonly property real ringScale: AppMetrics.welcomeRingSize / AppMetrics.goalRingSize
    readonly property real textGap: AppMetrics.gapLarge * root.ringScale
    readonly property real compositionLeft: (root.width - AppMetrics.welcomeRingSize - root.textGap - lines.width) / 2

    property real entrance: 0
    property bool landing: false

    function phase(index)
    {
        if (AppMetrics.motionReveal === 0)
        {
            return 1
        }

        const start = index * AppMetrics.motionFast / AppMetrics.motionReveal
        const span = AppMetrics.motionSlow / AppMetrics.motionReveal
        const progress = Math.max(0, Math.min(1, (root.entrance - start) / span))

        return 1 - Math.pow(1 - progress, 3)
    }

    function dismiss()
    {
        if (!root.shown || exitAnimation.running)
        {
            return
        }

        holdTimer.stop()

        root.landing = root.target !== null && root.target.visible && root.target.width > 0

        if (root.landing)
        {
            const place = root.target.mapToItem(root, 0, 0)

            ringFlightX.to = place.x
            ringFlightY.to = place.y
            ringFlightScale.to = 1
        }
        else
        {
            ringFlightX.to = ring.x
            ringFlightY.to = ring.y
            ringFlightScale.to = ring.scale
        }

        console.info("Welcome screen dismissed, ring lands on Today:", root.landing)

        exitAnimation.start()
    }

    visible: root.shown
    focus: root.shown

    Accessible.role: Accessible.Pane
    Accessible.name: root.greeting

    Keys.onPressed: function(event)
    {
        event.accepted = true
        root.dismiss()
    }

    Rectangle
    {
        id: backdrop

        anchors.fill: parent
        color: AppTheme.colourSunken
    }

    MouseArea
    {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.AllButtons

        onClicked: root.dismiss()
        onWheel: function(wheel) { wheel.accepted = true }
    }

    DesktopDailyRing
    {
        id: ring

        x: root.compositionLeft + (AppMetrics.welcomeRingSize - width) / 2
        y: (root.height - height) / 2
        width: AppMetrics.goalRingSize
        height: AppMetrics.goalRingSize
        scale: root.ringScale
        opacity: root.phase(0)

        sweepDuration: AppMetrics.motionReveal
        outerProgress: root.entrance > 0 ? root.reviewProgress : 0
        innerProgress: root.entrance > 0 ? root.learnProgress : 0
        showOuter: root.showReviewRing
        showInner: root.showLearnRing
        doneToday: root.doneToday
        dailyGoal: root.dailyGoal
    }

    ColumnLayout
    {
        id: lines

        x: root.compositionLeft + AppMetrics.welcomeRingSize + root.textGap
        y: (root.height - height) / 2
        width: Math.min(implicitWidth, root.width - AppMetrics.welcomeRingSize - root.textGap - AppMetrics.contentPadding * 2)
        spacing: AppMetrics.gapSmall

        transform: Translate { id: linesDrift }

        DesktopSectionLabel
        {
            Layout.fillWidth: true
            text: Qt.locale().toString(new Date(), "dddd d MMMM")
            font.pixelSize: AppMetrics.fontMeta
            opacity: root.phase(1)

            transform: Translate { y: AppMetrics.motionTravel * (1 - root.phase(1)) }
        }

        DesktopLabel
        {
            Layout.fillWidth: true
            Layout.bottomMargin: AppMetrics.gapMedium
            text: root.greeting
            font.pixelSize: AppMetrics.welcomeGreetingSize
            font.weight: Font.Bold
            wrapMode: Text.WordWrap
            opacity: root.phase(2)

            transform: Translate { y: AppMetrics.motionTravel * (1 - root.phase(2)) }
        }

        Repeater
        {
            model: [
                { count: root.reviewCount, label: qsTr("to review"), colour: AppTheme.colourAccent },
                { count: root.newCount, label: qsTr("new words"), colour: AppTheme.colourCorrect }
            ]

            delegate: RowLayout
            {
                id: legendRow

                required property var modelData
                required property int index

                spacing: AppMetrics.gapSmall
                opacity: root.phase(3 + legendRow.index)

                transform: Translate { y: AppMetrics.motionTravel * (1 - root.phase(3 + legendRow.index)) }

                Rectangle
                {
                    Layout.alignment: Qt.AlignVCenter
                    implicitWidth: AppMetrics.masteryDotSize
                    implicitHeight: AppMetrics.masteryDotSize
                    radius: AppMetrics.masteryDotSize / 2
                    color: legendRow.modelData.colour
                }

                DesktopLabel
                {
                    text: legendRow.modelData.count
                    font.pixelSize: AppMetrics.fontTitle
                    font.weight: Font.Bold
                    color: legendRow.modelData.count > 0 ? AppTheme.colourText : AppTheme.colourMuted
                }

                DesktopLabel
                {
                    Layout.alignment: Qt.AlignBaseline
                    text: legendRow.modelData.label
                    font.pixelSize: AppMetrics.fontLead
                    color: AppTheme.colourSubtle
                }
            }
        }
    }

    NumberAnimation
    {
        id: entranceAnimation

        target: root
        property: "entrance"
        from: 0
        to: 1
        duration: AppMetrics.motionReveal
        running: root.shown
    }

    Timer
    {
        id: holdTimer

        interval: root.holdDuration
        running: root.shown && root.Window.window !== null && root.Window.window.visible

        onTriggered: root.dismiss()
    }

    SequentialAnimation
    {
        id: exitAnimation

        ParallelAnimation
        {
            NumberAnimation { target: lines; property: "opacity"; to: 0; duration: AppMetrics.motionSlow; easing.type: AppMetrics.motionEasing }
            NumberAnimation { target: linesDrift; property: "x"; to: -AppMetrics.motionTravel; duration: AppMetrics.motionSlow; easing.type: AppMetrics.motionEasing }
            NumberAnimation { target: ring; property: "opacity"; to: 1; duration: AppMetrics.motionSlow; easing.type: AppMetrics.motionEasing }
            NumberAnimation { id: ringFlightX; target: ring; property: "x"; duration: AppMetrics.motionReveal; easing.type: AppMetrics.motionEasing }
            NumberAnimation { id: ringFlightY; target: ring; property: "y"; duration: AppMetrics.motionReveal; easing.type: AppMetrics.motionEasing }
            NumberAnimation { id: ringFlightScale; target: ring; property: "scale"; duration: AppMetrics.motionReveal; easing.type: AppMetrics.motionEasing }
        }

        ParallelAnimation
        {
            NumberAnimation { target: backdrop; property: "opacity"; to: 0; duration: AppMetrics.motionSlow; easing.type: AppMetrics.motionEasing }
            NumberAnimation { target: ring; property: "opacity"; to: root.landing ? 1 : 0; duration: AppMetrics.motionSlow; easing.type: AppMetrics.motionEasing }
        }

        ScriptAction
        {
            script:
            {
                root.shown = false
                root.finished()
            }
        }
    }
}
