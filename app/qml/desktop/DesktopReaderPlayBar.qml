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
            objectName: "reader_read_aloud_previous"

            focusPolicy: Qt.NoFocus
            iconName: "skip_back"
            text: qsTr("Previous sentence (Ctrl+Left)")

            onClicked: root.readAloud.previous()
        }

        DesktopIconButton
        {
            objectName: "reader_read_aloud_play"

            focusPolicy: Qt.NoFocus
            iconName: root.readAloud.playing ? "pause" : "play"
            iconColour: AppTheme.colourAccent
            text: root.readAloud.playing ? qsTr("Pause (Space)") : qsTr("Play (Space)")

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
            objectName: "reader_read_aloud_next"

            focusPolicy: Qt.NoFocus
            iconName: "skip_forward"
            text: qsTr("Next sentence (Ctrl+Right)")

            onClicked: root.readAloud.next()
        }

        DesktopButton
        {
            objectName: "reader_read_aloud_speed"

            focusPolicy: Qt.NoFocus
            tone: "ghost"
            text: qsTr("%1%").arg(Math.round(root.readAloud.speed * 100))
            Accessible.description: qsTr("Reading speed")

            onClicked: root.readAloud.stepSpeed()
        }

        DesktopIconButton
        {
            objectName: "reader_read_aloud_close"

            focusPolicy: Qt.NoFocus
            iconName: "close"
            text: qsTr("Close")

            onClicked: root.readAloud.stop()
        }
    }
}
