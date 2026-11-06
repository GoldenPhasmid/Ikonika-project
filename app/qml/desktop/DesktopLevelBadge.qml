import QtQuick

Rectangle
{
    id: root

    property string label: ""
    property color fill: AppTheme.colourBorder

    readonly property bool darkSurface: AppTheme.colourPanel.hslLightness < 0.5

    implicitWidth: text.implicitWidth + AppMetrics.gapMedium
    implicitHeight: AppMetrics.badgeHeight

    visible: root.label.length > 0
    radius: AppMetrics.radiusSmall
    color: Qt.tint(AppTheme.colourPanel, AppTheme.faded(root.fill, AppTheme.alphaMedium))

    Accessible.role: Accessible.StaticText
    Accessible.name: root.label

    DesktopLabel
    {
        id: text

        anchors.centerIn: parent

        text: root.label
        font.pixelSize: AppMetrics.fontMicro
        font.weight: Font.Bold
        color: root.darkSurface ? root.fill : Qt.darker(root.fill, 1.8)
    }
}
