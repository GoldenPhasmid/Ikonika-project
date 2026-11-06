import QtQuick

Rectangle
{
    id: root

    property string label: ""

    visible: root.label.length > 0
    implicitWidth: text.implicitWidth + AppMetrics.gapMedium
    implicitHeight: text.implicitHeight + AppMetrics.gapTight * 2
    radius: AppMetrics.radiusSmall
    color: Qt.tint(AppTheme.colourPanel, AppTheme.faded(AppTheme.colourAccent, AppTheme.alphaMedium))

    Accessible.role: Accessible.StaticText
    Accessible.name: root.label

    DesktopLabel
    {
        id: text

        anchors.centerIn: parent
        text: root.label
        font.pixelSize: AppMetrics.fontSmall
        font.weight: Font.DemiBold
        color: AppTheme.colourAccent
    }
}
