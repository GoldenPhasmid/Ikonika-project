import QtQuick
import QtQuick.Window

Window
{
    title: qsTr("Ikonika")
    color: AppTheme.colourPanel

    LoadingScreen
    {
        anchors.fill: parent
        message: qsTr("Switching profile")
    }
}
