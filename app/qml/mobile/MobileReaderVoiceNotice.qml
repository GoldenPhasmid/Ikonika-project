import QtQuick
import QtQuick.Layouts
import "MobileIcons.js" as Icons

Rectangle
{
    id: root

    required property ReaderReadAloud readAloud

    signal closed()

    implicitWidth: row.implicitWidth + AppMetrics.gapMedium + AppMetrics.gapSmall
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

        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: AppMetrics.gapMedium
        spacing: AppMetrics.gapSmall

        Text
        {
            text: root.readAloud.voiceInstalled ? qsTr("Speech is turned off") : qsTr("No voice is installed")
            color: AppTheme.colourMuted
            font.pixelSize: AppMetrics.fontBody
            font.family: AppMetrics.fontFamily
        }

        Text
        {
            objectName: "reader_voice_turn_on"

            Layout.fillHeight: true
            visible: root.readAloud.voiceInstalled
            leftPadding: AppMetrics.gapSmall
            rightPadding: AppMetrics.gapSmall
            text: qsTr("Turn on")
            color: AppTheme.colourAccent
            font.pixelSize: AppMetrics.fontBody
            font.weight: Font.DemiBold
            font.family: AppMetrics.fontFamily
            verticalAlignment: Text.AlignVCenter

            MouseArea
            {
                anchors.fill: parent

                onClicked: root.readAloud.enableSpeech()
            }
        }

        MobileActionIconButton
        {
            objectName: "reader_voice_close"

            iconSvg: Icons.Icons.close.path

            onClicked: root.closed()
        }
    }
}
