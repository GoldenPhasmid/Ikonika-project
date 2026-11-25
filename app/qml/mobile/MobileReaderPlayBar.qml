import QtQuick
import QtQuick.Layouts

Rectangle
{
    id: root

    required property ReaderReadAloud readAloud

    signal playRequested()

    implicitWidth: row.implicitWidth
    implicitHeight: row.implicitHeight
    radius: AppMetrics.radiusSmall
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

        MobileIconButton
        {
            objectName: "reader_read_aloud_previous"

            iconName: "skip_back"
            text: qsTr("Previous")

            onClicked: root.readAloud.previous()
        }

        MobileIconButton
        {
            objectName: "reader_read_aloud_play"

            iconName: root.readAloud.playing ? "pause" : "play"
            iconColour: AppTheme.colourAccent
            text: root.readAloud.playing ? qsTr("Pause") : qsTr("Play")

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

        MobileIconButton
        {
            objectName: "reader_read_aloud_next"

            iconName: "skip_forward"
            text: qsTr("Next")

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

        MobileIconButton
        {
            objectName: "reader_read_aloud_close"

            iconName: "close"
            text: qsTr("Stop reading aloud")

            onClicked: root.readAloud.stop()
        }
    }
}
