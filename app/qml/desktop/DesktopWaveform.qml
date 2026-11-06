import QtQuick

Item
{
    id: root

    property bool playing: false
    property color color: AppTheme.colourAccent

    readonly property var barLevels: [0.16, 0.24, 0.36, 0.5, 0.42, 0.62, 0.85, 1.0, 0.9, 0.7, 0.88, 0.95, 0.72, 0.55, 0.68, 0.45, 0.34, 0.5, 0.28, 0.2, 0.14]

    readonly property real iconShare: 0.45
    readonly property real barShare: 0.55
    readonly property real rowShare: 0.7
    readonly property real restPulse: 0.45
    readonly property real phaseStep: 0.7

    readonly property real contentHeight: Math.min(root.height, AppMetrics.buttonHeightLarge)

    property real wavePhase: 0

    NumberAnimation on wavePhase
    {
        running: root.playing && root.visible
        from: 0
        to: Math.PI * 2
        duration: AppMetrics.waveformCycle
        loops: Animation.Infinite
    }

    Row
    {
        id: contentRow

        anchors.centerIn: parent
        spacing: AppMetrics.gapCompact

        DesktopIcon
        {
            id: speakerIcon

            anchors.verticalCenter: parent.verticalCenter
            width: root.contentHeight * root.iconShare
            height: width
            name: "speaker_on"
            color: root.color
            filled: true
        }

        Row
        {
            id: bars

            readonly property int barCount: Math.max(5, Math.min(root.barLevels.length,
                Math.floor(bars.width / (AppMetrics.waveformBarWidth * 2))))

            anchors.verticalCenter: parent.verticalCenter
            width: root.width * root.rowShare - speakerIcon.width - contentRow.spacing
            height: root.contentHeight * root.barShare
            spacing: (bars.width - bars.barCount * AppMetrics.waveformBarWidth) / (bars.barCount - 1)

            Repeater
            {
                model: bars.barCount

                delegate: Rectangle
                {
                    required property int index

                    readonly property real level: root.barLevels[
                        Math.round(index * (root.barLevels.length - 1) / (bars.barCount - 1))]

                    readonly property real pulse: root.playing
                        ? root.restPulse + (1 - root.restPulse) * Math.abs(Math.sin(root.wavePhase + index * root.phaseStep))
                        : 1

                    anchors.verticalCenter: parent.verticalCenter
                    width: AppMetrics.waveformBarWidth
                    height: Math.max(AppMetrics.waveformBarMinimumHeight, bars.height * level * pulse)
                    radius: width / 2
                    color: root.color
                }
            }
        }
    }
}
