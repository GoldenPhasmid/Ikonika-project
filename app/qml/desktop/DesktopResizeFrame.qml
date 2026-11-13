import QtQuick

Item
{
    id: root

    required property Window window

    readonly property real edge: AppMetrics.gapSmall

    function start(edges)
    {
        root.window.startSystemResize(edges)
    }

    anchors.fill: parent
    z: 2

    Accessible.ignored: true

    MouseArea
    {
        x: root.edge
        width: parent.width - root.edge * 2
        height: root.edge
        cursorShape: Qt.SizeVerCursor

        onPressed: root.start(Qt.TopEdge)
    }

    MouseArea
    {
        x: root.edge
        y: parent.height - root.edge
        width: parent.width - root.edge * 2
        height: root.edge
        cursorShape: Qt.SizeVerCursor

        onPressed: root.start(Qt.BottomEdge)
    }

    MouseArea
    {
        y: root.edge
        width: root.edge
        height: parent.height - root.edge * 2
        cursorShape: Qt.SizeHorCursor

        onPressed: root.start(Qt.LeftEdge)
    }

    MouseArea
    {
        x: parent.width - root.edge
        y: root.edge
        width: root.edge
        height: parent.height - root.edge * 2
        cursorShape: Qt.SizeHorCursor

        onPressed: root.start(Qt.RightEdge)
    }

    MouseArea
    {
        width: root.edge
        height: root.edge
        cursorShape: Qt.SizeFDiagCursor

        onPressed: root.start(Qt.TopEdge | Qt.LeftEdge)
    }

    MouseArea
    {
        x: parent.width - root.edge
        y: parent.height - root.edge
        width: root.edge
        height: root.edge
        cursorShape: Qt.SizeFDiagCursor

        onPressed: root.start(Qt.BottomEdge | Qt.RightEdge)
    }

    MouseArea
    {
        x: parent.width - root.edge
        width: root.edge
        height: root.edge
        cursorShape: Qt.SizeBDiagCursor

        onPressed: root.start(Qt.TopEdge | Qt.RightEdge)
    }

    MouseArea
    {
        y: parent.height - root.edge
        width: root.edge
        height: root.edge
        cursorShape: Qt.SizeBDiagCursor

        onPressed: root.start(Qt.BottomEdge | Qt.LeftEdge)
    }
}
