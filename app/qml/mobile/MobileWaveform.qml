import QtQuick
import "MobileIcons.js" as Icons

Item
{
    id: root

    property bool playing: false
    property color color: AppTheme.colourAccent

    readonly property var barLevels: [0.16, 0.24, 0.36, 0.5, 0.42, 0.62, 0.85, 1.0, 0.9, 0.7, 0.88, 0.95, 0.72, 0.55, 0.68, 0.45, 0.34, 0.5, 0.28, 0.2, 0.14]

    readonly property real contentHeight: Math.min(root.height, AppMetrics.buttonHeightLarge)

    property real wavePhase: 0

    NumberAnimation on wavePhase
    {
        running: root.playing
        from: 0
        to: Math.PI * 2
        duration: 700
        loops: Animation.Infinite
    }

    Row
    {
        id: contentRow

        anchors.centerIn: parent
        spacing: AppMetrics.gapCompact

        MobileVectorIcon
        {
            id: speakerIcon

            anchors.verticalCenter: parent.verticalCenter

            width: root.contentHeight * 0.45
            height: width

            svgPath: Icons.Icons.speaker_on.path
            color: root.color
            filled: true
            strokeWidth: 1.8
        }

        Row
        {
            id: bars

            anchors.verticalCenter: parent.verticalCenter
            width: root.width * 0.7 - speakerIcon.width - contentRow.spacing
            height: root.contentHeight * 0.55
            spacing: (width - bars.barCount * 3) / (bars.barCount - 1)

            readonly property int barCount: Math.max(5, Math.min(root.barLevels.length, Math.floor(width / 6)))

            Repeater
            {
                model: bars.barCount

                delegate: Rectangle
                {
                    required property int index

                    readonly property real level: root.barLevels[
                        Math.round(index * (root.barLevels.length - 1) / (bars.barCount - 1))]

                    readonly property real pulse: root.playing
                        ? 0.45 + 0.55 * Math.abs(Math.sin(root.wavePhase + index * 0.7))
                        : 1

                    anchors.verticalCenter: parent.verticalCenter
                    width: 3
                    height: Math.max(4, bars.height * level * pulse)
                    radius: width / 2
                    color: root.color
                }
            }
        }
    }
}
