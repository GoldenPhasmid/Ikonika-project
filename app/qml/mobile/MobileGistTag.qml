import QtQuick

Rectangle
{
    id: root

    property string text: ""

    visible: root.text.length > 0
    implicitWidth: label.implicitWidth + AppMetrics.gapMedium
    implicitHeight: label.implicitHeight + AppMetrics.gapTight * 2
    radius: AppMetrics.radiusSmall
    color: Qt.rgba(AppTheme.colourAccent.r, AppTheme.colourAccent.g, AppTheme.colourAccent.b, 0.18)

    Text
    {
        id: label

        anchors.centerIn: parent
        text: root.text
        font.pixelSize: AppMetrics.fontMeta
        font.weight: Font.DemiBold
        font.family: AppMetrics.fontFamily
        color: AppTheme.colourAccent
    }
}
