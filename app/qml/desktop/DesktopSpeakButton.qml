import QtQuick

DesktopIconButton
{
    id: root

    property bool requested: false

    signal speakRequested()

    function speak()
    {
        root.requested = true
        root.speakRequested()
    }

    readonly property bool playing: root.requested && AppSpeech.speaking

    iconName: "speaker_on"
    active: root.playing
    opacity: root.enabled ? pulse.level : AppTheme.opacityDisabled

    onClicked: root.speak()

    Connections
    {
        target: AppSpeech

        function onSpeakingChanged(bSpeaking)
        {
            if (!bSpeaking)
            {
                root.requested = false
            }
        }
    }

    QtObject
    {
        id: pulse

        property real level: 1.0
    }

    SequentialAnimation
    {
        running: root.playing
        loops: Animation.Infinite

        onStopped: pulse.level = 1.0

        NumberAnimation { target: pulse; property: "level"; to: AppTheme.opacitySecondary; duration: AppMetrics.waveformCycle / 2; easing.type: Easing.InOutSine }
        NumberAnimation { target: pulse; property: "level"; to: 1.0; duration: AppMetrics.waveformCycle / 2; easing.type: Easing.InOutSine }
    }
}
