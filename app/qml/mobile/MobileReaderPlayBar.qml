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
            objectName: "reader_read_aloud_previous"

            iconSvg: Icons.Icons.skip_back.path

            onClicked: root.readAloud.previous()
        }

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
            objectName: "reader_read_aloud_next"

            iconSvg: Icons.Icons.skip_forward.path

            onClicked: root.readAloud.next()
        }

        Text
        {
            objectName: "reader_read_aloud_speed"

            Layout.preferredWidth: speedMetrics.advanceWidth + AppMetrics.gapSmall * 2
            Layout.fillHeight: true
            text: qsTr("%1%").arg(Math.round(root.readAloud.speed * 100))
            color: AppTheme.colourText
            font.pixelSize: AppMetrics.fontBody
            font.weight: Font.Medium
            font.family: AppMetrics.fontFamily
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter

            TextMetrics
            {
                id: speedMetrics

                font.pixelSize: AppMetrics.fontBody
                font.weight: Font.Medium
                font.family: AppMetrics.fontFamily
                text: "150%"
            }

            MouseArea
            {
                anchors.fill: parent

                onClicked: root.readAloud.stepSpeed()
            }
        }

        MobileActionIconButton
        {
            objectName: "reader_read_aloud_close"

            iconSvg: Icons.Icons.close.path

            onClicked: root.readAloud.stop()
        }
    }
}
