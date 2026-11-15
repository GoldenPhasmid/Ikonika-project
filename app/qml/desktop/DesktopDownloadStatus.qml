import QtQuick

QtObject
{
    id: root

    property bool available: false
    property string name
    property string title
    property string stateText
    property string detail
    property string error
    property string sizeText
    property real progress: -1

    readonly property string percentText: qsTr("%1%").arg(Math.round(Math.max(root.progress, 0) * 100))

    function megabytes(bytes)
    {
        return Math.round(bytes / (1024 * 1024))
    }

    function start()
    {
    }
}
