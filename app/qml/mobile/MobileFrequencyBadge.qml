import QtQuick

Rectangle
{
    id: root

    required property string bucket

    color: AppTheme.frequencyColours[root.bucket] || AppTheme.colourBorder
    radius: AppMetrics.radiusSmall
    implicitWidth: label.implicitWidth + 16
    implicitHeight: 22

    Text
    {
        id: label

        anchors.centerIn: parent

        text: root.bucket
        font.pixelSize: 11
        font.bold: true
        font.family: AppMetrics.fontFamily
        color: "#16181d"
    }
}
