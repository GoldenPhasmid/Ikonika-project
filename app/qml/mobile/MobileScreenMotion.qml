import QtQuick

Rectangle
{
    id: root

    required property Item target
    required property string screenName
    required property var router

    readonly property bool current: root.router.screen === root.screenName
    readonly property real span: root.parent ? root.parent.width : 0
    readonly property real underlap: 0.3

    property bool lingering: false
    property bool raised: false
    property real shift: 0

    x: root.shift
    z: 1
    width: root.span
    height: root.parent ? root.parent.height : 0
    visible: root.raised
    color: AppTheme.colourPanel

    function settle()
    {
        root.lingering = false
        root.raised = false
        root.shift = 0
    }

    onCurrentChanged:
    {
        const direction = root.router.transitionDirection

        slideMotion.stop()

        if (direction === 0 || AppMetrics.motionTransition === 0)
        {
            root.settle()

            return
        }

        if (root.current)
        {
            root.lingering = false
            root.raised = direction > 0
            root.shift = direction > 0 ? root.span : -root.span * root.underlap
            slideMotion.to = 0
        }
        else
        {
            root.lingering = true
            root.raised = direction < 0
            slideMotion.to = direction > 0 ? -root.span * root.underlap : root.span
        }

        slideMotion.start()
    }

    NumberAnimation
    {
        id: slideMotion

        target: root
        property: "shift"
        duration: AppMetrics.motionTransition
        easing.type: AppMetrics.motionEasing

        onRunningChanged:
        {
            if (running)
            {
                AppSpeech.holdSpeech()

                return
            }

            AppSpeech.releaseSpeech()
        }

        onFinished: root.settle()
    }

    Translate
    {
        id: slide

        x: root.shift
    }

    Component.onCompleted:
    {
        const transforms = []

        for (let index = 0; index < root.target.transform.length; ++index)
        {
            transforms.push(root.target.transform[index])
        }

        transforms.push(slide)
        root.target.transform = transforms
    }

    Binding { target: root.target; property: "visible"; value: root.current || root.lingering }
    Binding { target: root.target; property: "enabled"; value: root.current }
    Binding { target: root.target; property: "z"; value: root.raised ? 2 : 0 }
}
