import QtQuick

Loader
{
    id: root

    LoadingScreen
    {
        anchors.fill: parent
        z: 1
        visible: root.status === Loader.Loading
    }
}
