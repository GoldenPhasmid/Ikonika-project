import QtQuick
import QtQuick.Layouts

Rectangle
{
    id: root

    property bool interactive: true
    property bool playing: false

    signal clicked()

    Layout.fillWidth: true
    Layout.preferredHeight: AppMetrics.buttonHeightLarge

    radius: AppMetrics.radiusMedium

    color: AppTheme.colourPanel
    border.color: AppTheme.colourBorder
    border.width: AppMetrics.borderWidth
    opacity: root.interactive ? 1.0 : 0.5
    scale: tap.pressed ? 0.97 : 1

    Behavior on scale
    {
        NumberAnimation { duration: AppMetrics.motionFast; easing.type: AppMetrics.motionEasing }
    }

    MobileWaveform
    {
        anchors.fill: parent
        playing: root.playing
    }

    TapHandler
    {
        id: tap

        enabled: root.interactive

        onTapped: root.clicked()
    }
}
