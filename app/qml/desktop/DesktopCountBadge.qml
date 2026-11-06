import QtQuick

Rectangle
{
    id: root

    property int count: 0
    property bool emphasised: false

    implicitWidth: Math.max(AppMetrics.badgeHeight, label.implicitWidth + AppMetrics.gapCompact)
    implicitHeight: AppMetrics.badgeHeight

    visible: root.count > 0
    radius: AppMetrics.badgeHeight / 2
    color: root.emphasised ? AppTheme.colourAccent : AppTheme.colourFaint

    DesktopLabel
    {
        id: label

        anchors.centerIn: parent

        text: root.count
        font.pixelSize: AppMetrics.fontMicro
        font.weight: Font.DemiBold
        color: root.emphasised ? AppTheme.colourAccentText : AppTheme.colourSubtle
    }
}
