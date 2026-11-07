import QtQuick
import QtQuick.Layouts

Rectangle
{
    id: root

    property bool revealed: false

    default property alias content: contentColumn.data

    Layout.fillWidth: true
    implicitHeight: Math.max(AppMetrics.exerciseAnswerHeight,
        root.revealed ? contentColumn.implicitHeight + AppMetrics.gapLarge * 2 : 0)
    clip: true

    radius: AppMetrics.radiusMedium
    color: AppTheme.colourWell
    border.width: root.revealed ? 0 : AppMetrics.borderWidth
    border.color: AppTheme.colourBorder

    ColumnLayout
    {
        id: contentColumn

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.margins: AppMetrics.gapLarge
        visible: root.revealed
        spacing: AppMetrics.gapMedium
    }

    DesktopLabel
    {
        anchors.centerIn: parent
        visible: !root.revealed
        text: qsTr("?")
        font.pixelSize: AppMetrics.fontHeadline
        font.weight: Font.Light
        color: AppTheme.colourMuted

        Accessible.ignored: true
    }
}
