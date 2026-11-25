import QtQuick
import QtQuick.Layouts

ColumnLayout
{
    id: root

    property var source: AppMachineTranslation
    property alias buttonName: button.objectName
    property alias tone: button.tone

    readonly property real fullWidth: button.fullWidth

    spacing: AppMetrics.gapSmall

    DesktopButton
    {
        id: button

        Layout.fillWidth: true
        visible: !root.source.downloading
        focusPolicy: Qt.NoFocus
        tone: "primary"
        iconName: "cloud_download"
        text: root.source.bytes > 0
            ? qsTr("Download translation · %1 MB").arg(Math.round(root.source.bytes / (1024 * 1024)))
            : qsTr("Download translation")

        onClicked: root.source.download()
    }

    RowLayout
    {
        Layout.fillWidth: true
        visible: root.source.downloading
        spacing: AppMetrics.gapSmall

        DesktopProgressBar
        {
            Layout.fillWidth: true
            fill: AppTheme.colourAccent
            progress: root.source.progress
        }

        DesktopLabel
        {
            text: qsTr("%1%").arg(Math.round(root.source.progress * 100))
            font.pixelSize: AppMetrics.fontSmall
            color: AppTheme.colourMuted
        }
    }
}
