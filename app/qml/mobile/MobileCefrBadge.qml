import QtQuick

Rectangle
{
    id: root

    required property string level

    color: AppTheme.cefrColours[root.level] || AppTheme.colourBorder
    radius: AppMetrics.radiusSmall
    implicitWidth: label.implicitWidth + 16
    implicitHeight: 22

    Text
    {
        id: label

        anchors.centerIn: parent

        text: root.level
        font.pixelSize: AppMetrics.fontMicro
        font.bold: true
        font.family: AppMetrics.fontFamily
        font.features: { "tnum": 1 }
        color: "#16181d"
    }
}
