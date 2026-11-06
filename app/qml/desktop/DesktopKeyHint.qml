import QtQuick

Rectangle
{
    id: root

    property string key: ""
    property color tint: AppTheme.colourMuted

    implicitWidth: Math.max(AppMetrics.keyHintSize, label.implicitWidth + AppMetrics.gapSmall)
    implicitHeight: AppMetrics.keyHintSize

    radius: AppMetrics.radiusSmall
    color: "transparent"
    border.width: AppMetrics.borderWidth
    border.color: root.tint

    DesktopLabel
    {
        id: label

        anchors.centerIn: parent

        text: root.key
        font.pixelSize: AppMetrics.fontMicro
        font.weight: Font.DemiBold
        color: root.tint
    }
}
