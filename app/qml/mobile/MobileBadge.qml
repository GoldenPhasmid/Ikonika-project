import QtQuick

Rectangle
{
    id: root

    required property string label

    implicitWidth: text.implicitWidth + AppMetrics.gapCompact
    implicitHeight: 17

    radius: 4
    color: AppTheme.colourFaint

    Text
    {
        id: text

        anchors.centerIn: parent

        text: root.label
        font.pixelSize: AppMetrics.fontMicro
        font.family: AppMetrics.fontFamily
        color: AppTheme.colourSubtle
    }
}
