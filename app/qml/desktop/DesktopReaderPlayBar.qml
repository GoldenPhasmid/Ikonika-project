import QtQuick
import QtQuick.Layouts

Rectangle
{
    id: root

    required property ReaderReadAloud readAloud

    signal playRequested()

    implicitWidth: row.implicitWidth + AppMetrics.gapTight * 2
    implicitHeight: row.implicitHeight + AppMetrics.gapTight * 2
    radius: height / 2
    color: AppTheme.colourCard
    border.width: AppMetrics.borderWidth
    border.color: AppTheme.colourBorder

    Accessible.role: Accessible.ToolBar
    Accessible.name: qsTr("Read aloud")

    RowLayout
    {
        id: row

        anchors.centerIn: parent
        spacing: AppMetrics.gapTight

        DesktopIconButton
        {
            objectName: "reader_read_aloud_play"

            iconName: root.readAloud.playing ? "pause" : "play"
            iconColour: AppTheme.colourAccent
            text: root.readAloud.playing ? qsTr("Pause") : qsTr("Read aloud")

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

        DesktopIconButton
        {
            objectName: "reader_read_aloud_stop"

            iconName: "stop"
            text: qsTr("Stop")
            enabled: root.readAloud.started

            onClicked: root.readAloud.stop()
        }
    }
}
