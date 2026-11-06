import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Button
{
    id: root

    property bool playing: false
    property string keyHint: ""

    Layout.alignment: Qt.AlignHCenter
    implicitWidth: AppMetrics.waveformButtonWidth
    implicitHeight: AppMetrics.waveformButtonHeight

    padding: 0
    hoverEnabled: true
    focusPolicy: Qt.NoFocus
    text: qsTr("Play the word")

    Accessible.name: root.keyHint.length > 0 ? qsTr("%1 (%2)").arg(root.text).arg(root.keyHint) : root.text

    contentItem: Item
    {
        DesktopWaveform
        {
            anchors.fill: parent
            anchors.rightMargin: keyHintChip.visible ? keyHintChip.width + AppMetrics.gapMedium : 0
            playing: root.playing
        }

        DesktopKeyHint
        {
            id: keyHintChip

            anchors.right: parent.right
            anchors.rightMargin: AppMetrics.gapMedium
            anchors.verticalCenter: parent.verticalCenter
            visible: root.keyHint.length > 0
            key: root.keyHint
        }
    }

    background: Rectangle
    {
        radius: AppMetrics.radiusMedium
        color: root.down ? AppTheme.colourFaint : (root.hovered ? AppTheme.colourSunken : AppTheme.colourPanel)
        border.width: AppMetrics.borderWidth
        border.color: root.playing ? AppTheme.colourAccent : (root.hovered ? AppTheme.colourOutline : AppTheme.colourBorder)

        Behavior on color { ColorAnimation { duration: AppMetrics.motionFast } }
    }

    HoverHandler
    {
        cursorShape: Qt.PointingHandCursor
    }
}
