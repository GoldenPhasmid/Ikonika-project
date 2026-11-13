import QtQuick
import QtQuick.Layouts

Rectangle
{
    id: root

    required property ReaderReadAloud readAloud

    signal closed()

    readonly property string voicePackage: root.readAloud.voicePackage
    readonly property bool downloading: root.voicePackage.length > 0
        && (AppPackageDownloader.activeId === root.voicePackage || AppPackageDownloader.queue.indexOf(root.voicePackage) >= 0)
    readonly property int percent: AppPackageDownloader.activeId === root.voicePackage && AppPackageDownloader.totalBytes > 0
        ? Math.round(AppPackageDownloader.receivedBytes * 100 / AppPackageDownloader.totalBytes) : 0

    implicitWidth: row.implicitWidth + AppMetrics.gapMedium + AppMetrics.gapTight
    implicitHeight: row.implicitHeight + AppMetrics.gapTight * 2
    radius: height / 2
    color: AppTheme.colourCard
    border.width: AppMetrics.borderWidth
    border.color: AppTheme.colourBorder

    RowLayout
    {
        id: row

        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: AppMetrics.gapMedium
        spacing: AppMetrics.gapSmall

        DesktopLabel
        {
            text: root.readAloud.voiceInstalled ? qsTr("Speech is turned off")
                : root.downloading ? qsTr("Downloading the voice, %1%").arg(root.percent)
                : qsTr("No voice is installed")
            color: AppTheme.colourMuted
        }

        DesktopButton
        {
            objectName: "reader_voice_turn_on"

            visible: root.readAloud.voiceInstalled
            tone: "primary"
            text: qsTr("Turn on")

            onClicked: root.readAloud.enableSpeech()
        }

        DesktopButton
        {
            objectName: "reader_voice_download"

            visible: !root.readAloud.voiceInstalled && !root.downloading && root.voicePackage.length > 0
            tone: "primary"
            text: qsTr("Download")

            onClicked: AppPackageDownloader.download(root.voicePackage)
        }

        DesktopIconButton
        {
            objectName: "reader_voice_close"

            iconName: "close"
            text: qsTr("Close")

            onClicked: root.closed()
        }
    }
}
