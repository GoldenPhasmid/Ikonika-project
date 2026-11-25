import QtQuick
import Ikonika

Item
{
    id: root

    required property bool todayActive
    required property bool goalComplete
    required property DailySessionController daily
    property var training: null
    property var review: null
    property bool tapEnabled: true
    property rect crop

    readonly property bool present: root.visible && root.todayActive
    readonly property bool ready: root.currentClip !== null
    readonly property bool cropped: root.crop.width > 0 && root.crop.height > 0
    readonly property var currentClip: player.clips[root.clipId] ?? null
    readonly property bool awakeIdle: root.clipId === "idle" && !root.goalReached
    readonly property bool playingIdleBreak: root.clipId === "interested" || root.clipId === "paw-lick"
    readonly property int frameNumber: Math.min(Math.floor(root.playbackFrame), root.currentClip?.frames ?? 1)
    readonly property bool blinking: blinkHold.running || (root.playingIdleBreak
        && (root.currentClip?.blinkFrames.includes(root.frameNumber) ?? false))
    readonly property bool reactionQueued: root.sessionReactionPending || root.idleBreak.length > 0
    readonly property var idleEyes: ["Eyes Normal", "Eyes Excited", "Eyes Happy"]
    readonly property int maximumBreathWait: 250
    property int learningDay: -1
    property int completedSessions: 0
    property bool goalReached: false
    property bool sessionReactionPending: false
    property bool nightEyes: false
    property bool finishingBreath: false
    property string idleBreak: ""
    property string clipId: ""
    property real playbackFrame: 1

    function refreshNightEyes()
    {
        const hour = new Date().getHours()
        root.nightEyes = hour >= 20 || hour < 6
    }

    function recordSessionCompleted()
    {
        player.recordSessionCompleted()
        Qt.callLater(root.syncProgress)
    }

    function syncProgress()
    {
        const progress = player.syncDay(-1, root.goalComplete)
        root.completedSessions = progress.sessions ?? 0
        root.goalReached = progress.goalReached === true
        root.sessionReactionPending = progress.reactionPending === true
        if (root.learningDay !== progress.learningDay)
        {
            root.learningDay = progress.learningDay
            playback.stop()
            wakeDelay.stop()
            blinkHold.stop()
            root.clipId = ""
            root.idleBreak = ""
        }

        if (!root.present || !player.clips.idle)
        {
            root.pausePlayback()
            wakeDelay.stop()
            blinkHold.stop()
            return
        }

        root.refreshNightEyes()
        if (root.goalReached)
        {
            wakeDelay.stop()
            if (root.clipId === "")
                root.playClip("idle-sleep")
            else if (root.clipId !== "active-to-sleep" && root.clipId !== "idle-sleep")
                root.playClip("active-to-sleep")
        }
        else if (progress.wake)
        {
            if (root.clipId === "")
                root.playClip("idle-sleep")
            if (!wakeDelay.running)
                wakeDelay.start()
        }
        else if (root.clipId === "")
            root.playClip("idle")

        root.finishBreath()
        root.resumePlayback()
    }

    function playClip(next)
    {
        const clip = player.clips[next]
        playback.stop()
        if (root.clipId !== next)
        {
            blinkHold.stop()
            console.info("Lufi animation", next)
        }
        root.clipId = next
        root.playbackFrame = 1
        root.finishingBreath = false
        const tempo = next === "idle" ? 0.94 + Math.random() * 0.12 : 1
        root.configurePlayback(1, clip.frames + 1, 1000 * clip.frames / (clip.fps * tempo))
        Qt.callLater(root.resumePlayback)
    }

    function configurePlayback(firstFrame, endFrame, duration)
    {
        playback.from = firstFrame
        playback.to = endFrame
        playback.duration = Math.max(1, Math.round(duration))
        playback.elapsed = 0
    }

    function resumePlayback()
    {
        if (!root.present || !root.ready || playback.running)
            return

        playback.startedAt = player.playbackTime() - playback.elapsed
        playback.start()
    }

    function pausePlayback()
    {
        if (!playback.running)
            return

        playback.elapsed = Math.min(player.playbackTime() - playback.startedAt, playback.duration)
        playback.stop()
    }

    function queueIdleBreak(clip)
    {
        if (root.present && root.awakeIdle && !root.reactionQueued)
        {
            root.idleBreak = clip
            root.finishBreath()
        }
    }

    function reactToTap()
    {
        root.queueIdleBreak(Math.random() < 0.5 ? "interested" : "paw-lick")
    }

    function finishBreath()
    {
        if (!root.awakeIdle || !root.present || !root.reactionQueued || root.finishingBreath || blinkHold.running)
            return

        const frame = root.playbackFrame
        if (frame <= 1 || frame >= root.currentClip.frames)
        {
            root.advance()
            return
        }

        const remaining = playback.duration * (playback.to - frame) / (playback.to - playback.from)
        if (remaining > root.maximumBreathWait)
        {
            playback.stop()
            root.finishingBreath = true
            root.configurePlayback(frame, root.currentClip.frames, root.maximumBreathWait)
            Qt.callLater(root.resumePlayback)
        }
    }

    function advance()
    {
        if (root.clipId === "active-to-sleep" || root.clipId === "idle-sleep")
        {
            root.playClip("idle-sleep")
            return
        }
        if (!root.awakeIdle || !root.reactionQueued)
        {
            root.playClip("idle")
            root.finishBreath()
            return
        }

        let next = root.idleBreak
        if (root.sessionReactionPending)
        {
            player.consumeSessionReaction()
            root.sessionReactionPending = false
            next = "interested"
        }
        root.idleBreak = ""
        root.playClip(next)
    }

    function randomDelay(minimum, maximum)
    {
        return Math.round(minimum + Math.random() * (maximum - minimum))
    }

    onPresentChanged: Qt.callLater(root.syncProgress)
    onGoalCompleteChanged: Qt.callLater(root.syncProgress)
    Component.onCompleted: Qt.callLater(root.syncProgress)

    activeFocusOnTab: root.present && root.awakeIdle
    Accessible.name: qsTr("Lufi")
    Accessible.role: root.awakeIdle ? Accessible.Button : Accessible.Graphic
    Accessible.onPressAction: root.reactToTap()
    Keys.onReturnPressed: root.reactToTap()
    Keys.onSpacePressed: root.reactToTap()

    Connections
    {
        target: root.daily
        function onCountsChanged() { Qt.callLater(root.syncProgress) }
    }

    Connections
    {
        target: root.training

        function onSessionEnded()
        {
            if (!root.training.formsSession)
                root.recordSessionCompleted()
        }
    }

    Connections
    {
        target: root.review
        function onSessionEnded() { root.recordSessionCompleted() }
    }

    TapHandler
    {
        enabled: root.tapEnabled && root.present && root.awakeIdle
        onTapped: root.reactToTap()
    }

    HoverHandler
    {
        enabled: root.tapEnabled && root.present && root.awakeIdle
        cursorShape: Qt.PointingHandCursor
    }

    LufiAnimation
    {
        id: player

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        width: !root.ready ? 0 : root.cropped ? root.width : root.height * root.currentClip.width / root.currentClip.height
        height: root.ready ? root.height : 0
        viewBox:
        {
            const clip = root.currentClip
            if (!clip)
                return Qt.rect(0, 0, 0, 0)
            if (!root.cropped)
                return Qt.rect(0, 0, clip.width, clip.height)

            return Qt.rect(root.crop.x * clip.width, root.crop.y * clip.height,
                           root.crop.width * clip.width, root.crop.height * clip.height)
        }
        clipName: root.clipId
        selectedEyes: root.awakeIdle || root.playingIdleBreak
            ? (root.blinking ? "Eyes Closed" : root.idleEyes[Math.min(root.completedSessions, root.idleEyes.length - 1)])
            : "Eyes Authored"
        nightEyes: root.nightEyes
        frame: root.frameNumber

        onClipsChanged: Qt.callLater(root.syncProgress)
    }

    FrameAnimation
    {
        id: playback

        property real from: 1
        property real to: 1
        property real duration: 1
        property real elapsed: 0
        property real startedAt: 0

        onTriggered:
        {
            const elapsed = player.playbackTime() - playback.startedAt
            if (elapsed >= playback.duration)
            {
                playback.stop()
                root.advance()
                return
            }

            root.playbackFrame = playback.from + (playback.to - playback.from) * elapsed / playback.duration
        }
    }

    Timer
    {
        id: wakeDelay

        interval: 2000
        onTriggered:
        {
            if (!root.present)
                return

            const progress = player.syncDay(root.learningDay, root.goalComplete)
            if (progress.wake && progress.learningDay === root.learningDay)
                root.playClip("sleep-to-awake")
            else
                root.syncProgress()
        }
    }

    Timer
    {
        interval: 1000
        repeat: true
        running: root.present
        onTriggered: root.refreshNightEyes()
    }

    Timer
    {
        interval: root.randomDelay(2500, 5500)
        repeat: true
        running: root.present && root.awakeIdle && !root.reactionQueued
        onTriggered:
        {
            interval = root.randomDelay(2500, 5500)
            blinkHold.restart()
        }
    }

    Timer
    {
        id: blinkHold

        interval: 160
        onTriggered: root.finishBreath()
    }

    Timer
    {
        interval: root.randomDelay(18000, 35000)
        repeat: true
        running: root.present && root.awakeIdle && !root.reactionQueued
        onTriggered:
        {
            interval = root.randomDelay(18000, 35000)
            root.queueIdleBreak("paw-lick")
        }
    }
}
