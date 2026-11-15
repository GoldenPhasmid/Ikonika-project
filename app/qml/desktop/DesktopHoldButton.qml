import QtQuick

DesktopButton
{
    id: root

    property string label: ""
    property string holdLabel: ""
    property real held: 0

    signal confirmed()

    tone: "dangerOutline"
    text: root.down || root.held > 0 ? root.holdLabel : root.label

    Accessible.name: root.holdLabel
    Accessible.onPressAction: root.confirmed()

    onPressed:
    {
        releaseAnimation.stop()
        holdAnimation.duration = AppMetrics.holdDuration * (1 - root.held)
        holdAnimation.start()
    }

    onReleased: root.letGo()
    onCanceled: root.letGo()

    function letGo()
    {
        if (!holdAnimation.running)
        {
            return
        }

        holdAnimation.stop()
        releaseAnimation.start()
    }

    NumberAnimation
    {
        id: holdAnimation

        target: root
        property: "held"
        to: 1

        onFinished:
        {
            root.held = 0
            root.confirmed()
        }
    }

    NumberAnimation
    {
        id: releaseAnimation

        target: root
        property: "held"
        to: 0
        duration: AppMetrics.motionFast
    }

    Rectangle
    {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: parent.width * root.held
        radius: AppMetrics.radiusMedium
        color: AppTheme.faded(AppTheme.colourWrong, AppTheme.alphaMedium)
        visible: root.held > 0
    }
}
