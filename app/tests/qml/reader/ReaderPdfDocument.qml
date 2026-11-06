import QtQuick

QtObject
{
    property string source: ""
    readonly property bool ready: source.length > 0
    readonly property bool failed: false
    readonly property int pageCount: 12
    signal outlineReady(var entries)

    function requestOutline()
    {
        outlineReady([])
    }
}
