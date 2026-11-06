import QtQuick

Rectangle
{
    id: root

    property int duration: AppMetrics.toastDuration

    function show(message)
    {
        label.text = message
        root.opacity = 1
        hideTimer.restart()
    }

    anchors.right: parent.right
    anchors.bottom: parent.bottom
    anchors.margins: AppMetrics.contentPadding

    width: Math.min(AppMetrics.toastWidth, label.implicitWidth + AppMetrics.gapLarge * 2)
    height: label.implicitHeight + AppMetrics.gapMedium * 2

    radius: AppMetrics.radiusMedium
    color: AppTheme.colourText
    opacity: 0
    visible: root.opacity > 0

    Accessible.role: Accessible.AlertMessage
    Accessible.name: label.text

    Behavior on opacity { NumberAnimation { duration: AppMetrics.motionBase } }

    DesktopLabel
    {
        id: label

        anchors.fill: parent
        anchors.margins: AppMetrics.gapMedium
        anchors.leftMargin: AppMetrics.gapLarge
        anchors.rightMargin: AppMetrics.gapLarge

        color: AppTheme.colourPanel
        wrapMode: Text.WordWrap
        verticalAlignment: Text.AlignVCenter
    }

    Timer
    {
        id: hideTimer

        interval: root.duration
        onTriggered: root.opacity = 0
    }

    MouseArea
    {
        anchors.fill: parent
        onClicked: root.opacity = 0
    }
}
