import QtQuick

Rectangle
{
    id: root

    property string message: ""
    property int duration: 2600

    function show(newMessage)
    {
        root.message = newMessage
        root.shown = true
        hideTimer.restart()
    }

    property bool shown: false

    anchors.horizontalCenter: parent.horizontalCenter
    anchors.bottom: parent.bottom
    anchors.bottomMargin: AppMetrics.gapLarge

    width: Math.min(label.implicitWidth + AppMetrics.contentPadding * 2,
                    parent.width - AppMetrics.contentPadding * 2)
    height: label.implicitHeight + AppMetrics.gapMedium * 2
    radius: height / 2

    color: AppTheme.colourText
    opacity: root.shown ? 1.0 : 0.0
    visible: opacity > 0

    Behavior on opacity
    {
        NumberAnimation { duration: AppMetrics.motionBase; easing.type: AppMetrics.motionEasing }
    }

    Timer
    {
        id: hideTimer

        interval: root.duration
        onTriggered: root.shown = false
    }

    Text
    {
        id: label

        anchors.centerIn: parent
        width: root.width - AppMetrics.contentPadding * 2

        text: root.message
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        font.pixelSize: AppMetrics.fontBody
        font.family: AppMetrics.fontFamily
        color: AppTheme.colourPanel
    }
}
