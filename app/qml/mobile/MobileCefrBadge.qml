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
        font.pixelSize: 11
        font.bold: true
        font.family: Qt.platform.os === "windows" ? "Consolas" : "monospace"
        color: "#16181d"
    }
}
