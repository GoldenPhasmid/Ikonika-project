import QtQuick

Item
{
    id: root

    property bool active: false
    property point anchorPoint: Qt.point(0, 0)
    property real pointerY: 0

    signal started()
    signal scrollRequested(real pixels)

    function start(position)
    {
        root.anchorPoint = position
        root.pointerY = position.y
        root.active = true
        root.started()
    }

    function stop()
    {
        root.active = false
    }

    z: 3

    FrameAnimation
    {
        running: root.active

        onTriggered:
        {
            const distance = root.pointerY - root.anchorPoint.y
            const beyond = Math.max(0, Math.abs(distance) - AppMetrics.readerAutoscrollDeadZone)

            if (beyond > 0)
            {
                root.scrollRequested(Math.sign(distance) * beyond * AppMetrics.readerAutoscrollSpeed * frameTime)
            }
        }
    }

    MouseArea
    {
        anchors.fill: parent
        z: 1
        visible: root.active
        hoverEnabled: true
        acceptedButtons: Qt.AllButtons
        cursorShape: Qt.SizeVerCursor

        onPositionChanged: function(mouse) { root.pointerY = mouse.y }
        onReleased: root.stop()
        onWheel: function(wheel) { root.stop() }
    }

    MouseArea
    {
        anchors.fill: parent
        z: 2
        acceptedButtons: Qt.MiddleButton

        onPressed: function(mouse)
        {
            if (root.active)
            {
                root.stop()

                return
            }

            root.start(Qt.point(mouse.x, mouse.y))
        }

        onPositionChanged: function(mouse) { root.pointerY = mouse.y }

        onReleased:
        {
            if (Math.abs(root.pointerY - root.anchorPoint.y) > AppMetrics.readerAutoscrollDeadZone)
            {
                root.stop()
            }
        }
    }

    Rectangle
    {
        z: 3
        visible: root.active
        x: root.anchorPoint.x - width / 2
        y: root.anchorPoint.y - height / 2
        width: AppMetrics.readerAutoscrollAnchorSize
        height: width
        radius: width / 2
        color: AppTheme.colourCard
        border.width: AppMetrics.borderWidth
        border.color: AppTheme.colourBorder

        Rectangle
        {
            anchors.centerIn: parent
            width: parent.width / 5
            height: width
            radius: width / 2
            color: AppTheme.colourText
        }
    }
}
