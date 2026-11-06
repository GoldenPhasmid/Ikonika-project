import QtQuick
import QtQuick.Layouts
import "MobileIcons.js" as Icons

Rectangle
{
    id: root

    required property ReaderReadAloud readAloud

    signal playRequested()

    implicitWidth: row.implicitWidth + AppMetrics.gapSmall * 2
    implicitHeight: row.implicitHeight + AppMetrics.gapSmall * 2
    radius: AppMetrics.radiusMedium
    color: AppTheme.colourCard
    border.width: AppMetrics.borderWidth
    border.color: AppTheme.colourBorder

    MouseArea
    {
        anchors.fill: parent
    }

    RowLayout
    {
        id: row

        anchors.centerIn: parent
        spacing: AppMetrics.gapSmall

        MobileActionIconButton
        {
            objectName: "reader_read_aloud_play"

            active: true
            iconSvg: root.readAloud.playing ? Icons.Icons.pause.path : Icons.Icons.play.path

            onClicked:
            {
                if (root.readAloud.playing)
                {
                    root.readAloud.pause()

                    return
                }

                root.playRequested()
            }
        }

        MobileActionIconButton
        {
            objectName: "reader_read_aloud_stop"

            iconSvg: Icons.Icons.stop.path
            interactive: root.readAloud.started

            onClicked: root.readAloud.stop()
        }
    }
}
