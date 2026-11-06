import QtQuick

Item
{
    property string source: ""
    property ReaderPdfDocument document
    readonly property bool ready: document ? document.ready : source.length > 0
    readonly property bool failed: false
    readonly property int pageCount: 12
    property int page: 0
    property real renderScale: 1
    readonly property size pageSize: Qt.size(page === 1 ? 580 : 600, page === 4 ? 1100 : 800)
    readonly property real cropLeft: 60
    readonly property real cropRight: pageSize.width - 60
    property color paperColour: "white"
    property color inkColour: "black"
    property bool prefetch: true
    property rect visibleArea
    signal cropChanged()
    signal outlineReady(var entries)
    signal rendered(bool detail)

    function selectWordAt(position)
    {
        return { text: "word", rectangles: [Qt.rect(position.x, position.y, 20, 10)] }
    }

    function selectionBetween(first, last)
    {
        return { text: "a phrase", rectangles: [Qt.rect(first.x, first.y, 50, 10)] }
    }

    function requestOutline()
    {
        outlineReady([])
    }
}
