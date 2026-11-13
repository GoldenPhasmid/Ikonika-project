import QtQuick
import QtQuick.Layouts

ColumnLayout
{
    id: root

    property alias buttonName: button.objectName
    property alias tone: button.tone

    readonly property real fullWidth: button.fullWidth

    spacing: AppMetrics.gapSmall

    DesktopButton
    {
        id: button

        Layout.fillWidth: true
        visible: !AppMachineTranslation.downloading
        focusPolicy: Qt.NoFocus
        tone: "primary"
        iconName: "cloud_download"
        text: AppMachineTranslation.bytes > 0
            ? qsTr("Download translation · %1 MB").arg(Math.round(AppMachineTranslation.bytes / (1024 * 1024)))
            : qsTr("Download translation")

        onClicked: AppMachineTranslation.download()
    }

    RowLayout
    {
        Layout.fillWidth: true
        visible: AppMachineTranslation.downloading
        spacing: AppMetrics.gapSmall

        DesktopProgressBar
        {
            Layout.fillWidth: true
            fill: AppTheme.colourAccent
            progress: AppMachineTranslation.progress
        }

        DesktopLabel
        {
            text: qsTr("%1%").arg(Math.round(AppMachineTranslation.progress * 100))
            font.pixelSize: AppMetrics.fontSmall
            color: AppTheme.colourMuted
        }
    }
}
