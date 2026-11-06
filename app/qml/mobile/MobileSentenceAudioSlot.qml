import QtQuick
import QtQuick.Layouts

Item
{
    id: root

    property bool revealed: false
    property bool canPlay: true
    property bool playing: false
    property string translation: ""

    signal replayRequested()

    Layout.fillWidth: true
    implicitHeight: root.revealed
        ? Math.max(AppMetrics.buttonHeightLarge, translationText.implicitHeight)
        : AppMetrics.buttonHeightLarge

    MobilePlayButton
    {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: AppMetrics.buttonHeightLarge

        visible: root.canPlay && !root.revealed
        playing: root.playing

        onClicked: root.replayRequested()
    }

    Text
    {
        id: translationText

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top

        visible: root.revealed
        text: root.translation
        font.pixelSize: AppMetrics.sessionOptionSize
        font.weight: Font.Medium
        font.family: AppMetrics.fontFamily
        color: AppTheme.colourSubtle
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
    }
}
