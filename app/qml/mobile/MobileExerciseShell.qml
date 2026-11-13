import QtQuick
import QtQuick.Layouts

Rectangle
{
    id: root

    property string exerciseName: ""
    property var store: null
    property var controller: null
    readonly property bool speechOn: !!root.store && root.store.audioEnabled
    readonly property bool muted: !root.speechOn || !root.store.autoSpeak

    property bool showMute: true
    property bool showOptions: true

    property bool showProgress: true
    property real progress: 0

    property var questionSideNames: []
    property int questionSideIndex: 0

    property bool inSession: false
    property bool showCompleteStage: true

    property bool optionsSheetOpen: false

    property bool showVoiceOption: false
    property bool voiceOn: false

    property bool showTypingOption: false
    property bool typingOn: false

    property real contentLeftMargin: AppMetrics.contentPadding
    property real contentRightMargin: AppMetrics.contentPadding
    property real contentTopMargin: AppMetrics.contentPadding
    property real contentBottomMargin: AppMetrics.gapLarge
    property real contentSpacing: AppMetrics.gapMedium

    readonly property int actionsHeight: AppMetrics.buttonHeightLarge
    property bool hasActions: true
    property bool cardEditable: false

    default property alias content: contentBody.data
    property alias actions: actionsContent.data

    signal exitRequested()
    signal endSessionRequested()
    signal editCardRequested()
    signal restartRequested()
    signal completeRequested()
    signal questionSideChosen(int index)
    signal swiped(int direction)
    signal voiceToggled()
    signal typingToggled()

    color: AppTheme.colourPanel

    property real contentShift: 0
    property real contentOpacity: 1
    property real contentScale: 1
    property real snapshotShift: 0
    property var snapshotGrab: null
    property bool swapping: false

    property bool swipeEnabled: false
    property bool swipeHints: false
    property color swipeLeftColour: AppTheme.colourWrong
    property color swipeRightColour: AppTheme.colourCorrect
    readonly property real swipeThreshold: 0.25
    readonly property real swipeTiltDegrees: 16
    readonly property real swipeLiftRatio: 0.08

    property bool tiltActive: false
    property real snapshotTilt: 0
    property real snapshotLift: 0

    readonly property real swipeRatio: root.width > 0 ? root.contentShift / root.width : 0
    readonly property real swipeTilt: root.tiltActive ? root.swipeRatio * root.swipeTiltDegrees : 0
    readonly property real swipeLift: root.tiltActive ? -Math.abs(root.contentShift) * root.swipeLiftRatio : 0
    readonly property color swipeFrameColour: root.swipeRatio < 0 ? root.swipeLeftColour : root.swipeRightColour
    readonly property real swipeHintStrength: root.swipeHints && root.tiltActive
        ? Math.min(1, Math.abs(root.swipeRatio) / root.swipeThreshold)
        : 0

    function swap(change)
    {
        root.leave(change, 1)
    }

    function swapBack(change)
    {
        root.leave(change, -1)
    }

    function swipeOut(change, direction)
    {
        const releasedTilt = root.swipeTilt
        const releasedLift = root.swipeLift

        root.capture(change, function(result, startShift)
        {
            root.showSnapshot(result, startShift)

            root.snapshotTilt = releasedTilt
            root.snapshotLift = releasedLift
            snapshotTiltFling.to = direction * root.swipeTiltDegrees
            snapshotLiftFling.to = -root.width * root.swipeLiftRatio

            root.tiltActive = false

            snapshotFling.to = direction * root.width
            root.contentShift = 0
            root.contentOpacity = 0
            root.contentScale = 0.96

            change()

            stackMotion.start()
        })
    }

    function enter()
    {
        if (!root.visible || AppMetrics.motionTransition === 0)
        {
            return
        }

        root.contentShift = root.width
        slideMotion.restart()
    }

    function leave(change, direction)
    {
        root.capture(change, function(result, startShift)
        {
            root.slide(result, change, direction, startShift)
        })
    }

    function capture(change, present)
    {
        if (root.swapping)
        {
            return
        }

        settleMotion.stop()

        if (!root.visible || AppMetrics.motionTransition === 0)
        {
            root.contentShift = 0
            change()

            return
        }

        root.swapping = true

        const startShift = root.contentShift

        const grabbing = contentBody.grabToImage(function(result)
        {
            present(result, startShift)
        })

        if (!grabbing)
        {
            root.swapping = false
            root.contentShift = 0
            change()
        }
    }

    function showSnapshot(result, startShift)
    {
        const origin = contentBody.mapToItem(root, 0, 0)

        root.snapshotGrab = result
        snapshot.x = origin.x
        snapshot.y = origin.y
        snapshot.width = contentBody.width
        snapshot.height = contentBody.height
        snapshot.source = result.url

        root.snapshotShift = startShift || 0
    }

    function slide(result, change, direction, startShift)
    {
        root.showSnapshot(result, startShift)

        root.contentShift = direction * root.width
        snapshotSlide.to = -direction * root.width

        change()

        slideMotion.start()
    }

    ParallelAnimation
    {
        id: slideMotion

        NumberAnimation
        {
            id: snapshotSlide

            target: root
            property: "snapshotShift"
            duration: AppMetrics.motionTransition
            easing.type: AppMetrics.motionEasing
        }

        NumberAnimation
        {
            target: root
            property: "contentShift"
            to: 0
            duration: AppMetrics.motionTransition
            easing.type: AppMetrics.motionEasing
        }

        onFinished: root.clearSnapshot()
    }

    ParallelAnimation
    {
        id: stackMotion

        NumberAnimation
        {
            id: snapshotFling

            target: root
            property: "snapshotShift"
            duration: AppMetrics.motionTransition
            easing.type: AppMetrics.motionEasing
        }

        NumberAnimation
        {
            id: snapshotTiltFling

            target: root
            property: "snapshotTilt"
            duration: AppMetrics.motionTransition
            easing.type: AppMetrics.motionEasing
        }

        NumberAnimation
        {
            id: snapshotLiftFling

            target: root
            property: "snapshotLift"
            duration: AppMetrics.motionTransition
            easing.type: AppMetrics.motionEasing
        }

        NumberAnimation
        {
            target: root
            property: "contentOpacity"
            to: 1
            duration: AppMetrics.motionTransition
            easing.type: AppMetrics.motionEasing
        }

        NumberAnimation
        {
            target: root
            property: "contentScale"
            to: 1
            duration: AppMetrics.motionTransition
            easing.type: AppMetrics.motionEasing
        }

        onFinished: root.clearSnapshot()
    }

    function clearSnapshot()
    {
        root.snapshotTilt = 0
        root.snapshotLift = 0
        snapshot.source = ""
        root.snapshotGrab = null
        root.swapping = false
    }

    NumberAnimation
    {
        id: settleMotion

        target: root
        property: "contentShift"
        to: 0
        duration: AppMetrics.motionBase
        easing.type: AppMetrics.motionEasing

        onFinished: root.tiltActive = false
    }

    ColumnLayout
    {
        anchors.fill: parent
        spacing: 0

        MobileHeader
        {
            title: root.exerciseName
            showBack: true
            showMute: root.showMute
            muted: root.muted
            showSlowSpeech: root.showMute && root.speechOn && root.controller !== null
            showOptions: root.showOptions

            onBackRequested: root.exitRequested()
            onMuteRequested: if (root.speechOn) root.store.autoSpeak = !root.store.autoSpeak
            onSlowSpeechRequested: if (root.controller) root.controller.speakSlowly()
            onOptionsRequested: root.optionsSheetOpen = true
        }

        ColumnLayout
        {
            id: contentColumn

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: root.contentLeftMargin
            Layout.rightMargin: root.contentRightMargin
            Layout.topMargin: root.contentTopMargin
            spacing: root.contentSpacing

            MobileDotProgress
            {
                Layout.fillWidth: true
                opacity: root.showProgress ? 1 : 0
                progress: root.progress
            }

            ColumnLayout
            {
                id: contentBody

                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: root.contentSpacing

                opacity: root.contentOpacity
                scale: root.contentScale
                transform: [
                    Rotation
                    {
                        angle: root.swipeTilt
                        origin.x: contentBody.width / 2
                        origin.y: contentBody.height
                    },
                    Translate { x: root.contentShift; y: root.swipeLift }
                ]

                DragHandler
                {
                    target: null
                    enabled: root.swipeEnabled && !root.swapping
                    yAxis.enabled: false

                    onActiveTranslationChanged: if (active) root.contentShift = activeTranslation.x

                    onActiveChanged:
                    {
                        if (active)
                        {
                            settleMotion.stop()
                            root.tiltActive = root.swipeHints

                            return
                        }

                        const shift = root.contentShift

                        if (Math.abs(shift) >= root.width * root.swipeThreshold)
                        {
                            root.swiped(shift > 0 ? 1 : -1)
                        }

                        if (!root.swapping)
                        {
                            settleMotion.start()
                        }
                    }
                }
            }
        }

        Item
        {
            Layout.fillWidth: true
            Layout.preferredHeight: root.hasActions ? root.actionsHeight : 0
            Layout.leftMargin: root.contentLeftMargin
            Layout.rightMargin: root.contentRightMargin
            Layout.topMargin: root.contentSpacing
            Layout.bottomMargin: root.contentBottomMargin

            ColumnLayout
            {
                id: actionsContent
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    Image
    {
        id: snapshot

        visible: root.swapping && snapshot.status === Image.Ready
        transform: [
            Rotation
            {
                angle: root.snapshotTilt
                origin.x: snapshot.width / 2
                origin.y: snapshot.height
            },
            Translate { x: root.snapshotShift; y: root.snapshotLift }
        ]
    }

    MobileSheetLoader
    {
        anchors.fill: parent
        open: root.optionsSheetOpen

        sourceComponent: MobileExerciseOptionsSheet
        {
            questionSideNames: root.questionSideNames
            questionSideIndex: root.questionSideIndex
            showStageActions: root.inSession
            showCompleteStage: root.showCompleteStage
            showEditCard: root.inSession && root.cardEditable
            showVoiceOption: root.showVoiceOption
            voiceOn: root.voiceOn
            voiceAvailable: root.speechOn
            showTypingOption: root.showTypingOption
            typingOn: root.typingOn

            onDismissed: root.optionsSheetOpen = false
            onVoiceToggled: root.voiceToggled()
            onTypingToggled: root.typingToggled()

            onQuestionSideChosen: function(index) { root.questionSideChosen(index) }

            onEditCardRequested:
            {
                root.optionsSheetOpen = false
                root.editCardRequested()
            }

            onRestartStageRequested:
            {
                root.optionsSheetOpen = false
                root.restartRequested()
            }

            onCompleteStageRequested:
            {
                root.optionsSheetOpen = false
                root.completeRequested()
            }

            onEndSessionRequested:
            {
                root.optionsSheetOpen = false
                root.endSessionRequested()
            }
        }
    }
}
