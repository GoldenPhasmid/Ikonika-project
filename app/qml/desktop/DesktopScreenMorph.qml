import QtQuick

Item
{
    id: root

    property Item landing: null

    property bool morphing: false
    property real shellBarHidden: 0
    property real shellContentHidden: 0
    property real pageShown: 1
    property real barShown: 1
    property real contentShown: 1

    property rect ghostArea
    property rect morphFrom
    property rect morphTo
    property color morphFromColour
    property color morphFromBorder
    property real morphFromRadius: 0
    property color morphToColour
    property color morphToBorder
    property real morphToRadius: 0
    property Item morphTarget: null
    property Item morphTargetSurface: null
    property var morphTargetRect: null
    property var morphDone: null

    readonly property bool ready: AppMetrics.motionMorph > 0 && !root.morphing && !!root.landing

    function rectInHost(item)
    {
        return item.mapToItem(root, 0, 0, item.width, item.height)
    }

    function borderOf(surface)
    {
        return surface.border.width > 0 ? surface.border.color : surface.color
    }

    function placeGhost(area, colour, border, radius)
    {
        root.ghostArea = area
        morphGhost.color = colour
        morphGhost.border.color = border
        morphGhost.radius = radius
        morphGhost.visible = true
    }

    function prepareIn(source, surface)
    {
        if (AppMetrics.motionMorph === 0 || !source || !surface)
        {
            return
        }

        root.morphFrom = root.rectInHost(source)
        root.morphFromColour = surface.color
        root.morphFromBorder = root.borderOf(surface)
        root.morphFromRadius = surface.radius

        morphFace.sourceItem = source.visible ? source : null
        morphFace.width = root.morphFrom.width
        morphFace.height = root.morphFrom.height
        morphFace.opacity = 1

        root.placeGhost(root.morphFrom, root.morphFromColour, root.morphFromBorder, root.morphFromRadius)
        root.shellBarHidden = 0
        root.shellContentHidden = 0
        root.pageShown = 0
        root.barShown = 0
        root.contentShown = 0
        root.morphing = true
    }

    function startIn()
    {
        const surface = root.landing

        if (!surface)
        {
            console.warn("Screen morph has no surface to land on")
            root.finish()

            return
        }

        root.morphTo = root.rectInHost(surface)
        root.morphToColour = surface.color
        root.morphToBorder = root.borderOf(surface)
        root.morphToRadius = surface.radius
        morphInAnimation.start()
    }

    function startOut(target, surface, done, targetRect)
    {
        root.morphTarget = target
        root.morphTargetSurface = surface
        root.morphTargetRect = targetRect || null
        root.morphDone = done
        root.shellBarHidden = 1
        root.shellContentHidden = 1
        root.morphing = true
        fadeOutAnimation.start()
    }

    function flyBack()
    {
        const surface = root.landing

        if (!surface)
        {
            root.finish()

            return
        }

        root.morphFrom = root.rectInHost(surface)
        root.morphFromColour = surface.color
        root.morphFromBorder = root.borderOf(surface)
        root.morphFromRadius = surface.radius
        root.morphTo = root.morphTargetRect ? root.morphTargetRect(root) : root.rectInHost(root.morphTarget)
        root.morphToColour = root.morphTargetSurface.color
        root.morphToBorder = root.borderOf(root.morphTargetSurface)
        root.morphToRadius = root.morphTargetSurface.radius

        morphFace.sourceItem = root.morphTarget
        morphFace.width = root.morphTo.width
        morphFace.height = root.morphTo.height
        morphFace.opacity = 0

        root.placeGhost(root.morphFrom, root.morphFromColour, root.morphFromBorder, root.morphFromRadius)
        morphOutAnimation.start()
    }

    function finish()
    {
        const done = root.morphDone

        root.morphDone = null
        root.morphTarget = null
        root.morphTargetSurface = null
        root.morphTargetRect = null
        morphGhost.visible = false
        morphFace.sourceItem = null
        root.contentShown = 1
        root.pageShown = 1
        root.barShown = 1

        if (done)
        {
            done()
        }

        root.morphing = false
    }

    onMorphingChanged:
    {
        if (root.morphing)
        {
            AppSpeech.holdSpeech()

            return
        }

        AppSpeech.releaseSpeech()
    }

    Rectangle
    {
        id: morphGhost

        x: root.ghostArea.x
        y: root.ghostArea.y
        width: root.ghostArea.width
        height: root.ghostArea.height
        visible: false
        border.width: AppMetrics.borderWidth
        clip: true

        ShaderEffectSource
        {
            id: morphFace

            hideSource: true
        }
    }

    ParallelAnimation
    {
        id: morphInAnimation

        PropertyAnimation { target: root; property: "ghostArea"; from: root.morphFrom; to: root.morphTo; duration: AppMetrics.motionMorph; easing.type: AppMetrics.motionEasing }
        NumberAnimation { target: morphGhost; property: "radius"; from: root.morphFromRadius; to: root.morphToRadius; duration: AppMetrics.motionMorph; easing.type: AppMetrics.motionEasing }
        ColorAnimation { target: morphGhost; property: "color"; from: root.morphFromColour; to: root.morphToColour; duration: AppMetrics.motionBase }
        ColorAnimation { target: morphGhost; property: "border.color"; from: root.morphFromBorder; to: root.morphToBorder; duration: AppMetrics.motionBase }
        NumberAnimation { target: morphFace; property: "opacity"; to: 0; duration: AppMetrics.motionFast }
        NumberAnimation { target: root; property: "shellContentHidden"; to: 1; duration: AppMetrics.motionSlow; easing.type: Easing.InCubic }
        NumberAnimation { target: root; property: "shellBarHidden"; to: 1; duration: AppMetrics.motionBase; easing.type: Easing.InCubic }
        NumberAnimation { target: root; property: "pageShown"; to: 1; duration: AppMetrics.motionMorph }

        SequentialAnimation
        {
            PauseAnimation { duration: AppMetrics.motionBase }
            NumberAnimation { target: root; property: "barShown"; to: 1; duration: AppMetrics.motionSlow; easing.type: AppMetrics.motionEasing }
        }

        onFinished: revealAnimation.start()
    }

    NumberAnimation
    {
        id: revealAnimation

        target: root
        property: "contentShown"
        to: 1
        duration: AppMetrics.motionBase

        onFinished: root.finish()
    }

    NumberAnimation
    {
        id: fadeOutAnimation

        target: root
        property: "contentShown"
        to: 0
        duration: AppMetrics.motionFast

        onFinished: root.flyBack()
    }

    ParallelAnimation
    {
        id: morphOutAnimation

        PropertyAnimation { target: root; property: "ghostArea"; from: root.morphFrom; to: root.morphTo; duration: AppMetrics.motionMorph; easing.type: AppMetrics.motionEasing }
        NumberAnimation { target: morphGhost; property: "radius"; from: root.morphFromRadius; to: root.morphToRadius; duration: AppMetrics.motionMorph; easing.type: AppMetrics.motionEasing }
        NumberAnimation { target: root; property: "pageShown"; to: 0; duration: AppMetrics.motionMorph }
        NumberAnimation { target: root; property: "barShown"; to: 0; duration: AppMetrics.motionBase; easing.type: Easing.InCubic }

        SequentialAnimation
        {
            PauseAnimation { duration: AppMetrics.motionMorph - AppMetrics.motionBase }

            ParallelAnimation
            {
                ColorAnimation { target: morphGhost; property: "color"; from: root.morphFromColour; to: root.morphToColour; duration: AppMetrics.motionBase }
                ColorAnimation { target: morphGhost; property: "border.color"; from: root.morphFromBorder; to: root.morphToBorder; duration: AppMetrics.motionBase }
                NumberAnimation { target: morphFace; property: "opacity"; to: 1; duration: AppMetrics.motionBase }
            }
        }

        SequentialAnimation
        {
            PauseAnimation { duration: AppMetrics.motionBase }
            NumberAnimation { target: root; property: "shellBarHidden"; to: 0; duration: AppMetrics.motionSlow; easing.type: AppMetrics.motionEasing }
        }

        SequentialAnimation
        {
            PauseAnimation { duration: AppMetrics.motionFast }
            NumberAnimation { target: root; property: "shellContentHidden"; to: 0; duration: AppMetrics.motionMorph - AppMetrics.motionFast; easing.type: AppMetrics.motionEasing }
        }

        onFinished: root.finish()
    }
}
