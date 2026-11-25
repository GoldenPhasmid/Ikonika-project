import QtQuick
import QtQuick.Controls.Basic

Slider
{
    id: root

    property int count: 0
    property int index: 0

    signal positionRequested(int position)

    readonly property int shownIndex: root.pressed ? Math.round(root.value) : root.index

    implicitHeight: AppMetrics.chipHeight

    from: 0
    to: Math.max(1, root.count - 1)
    stepSize: 1
    value: root.index
    enabled: root.count > 1
    focusPolicy: Qt.NoFocus
    hoverEnabled: true
    padding: 0

    Accessible.name: qsTr("Position in the book")

    onPressedChanged:
    {
        if (!root.pressed)
        {
            root.positionRequested(Math.round(root.value))
            root.value = Qt.binding(function() { return root.index })
        }
    }

    background: Item
    {
        y: root.availableHeight / 2 - height / 2
        width: root.availableWidth
        height: AppMetrics.progressBarHeight

        Rectangle
        {
            anchors.fill: parent
            radius: height / 2
            color: AppTheme.colourFaint
        }

        Rectangle
        {
            width: root.visualPosition * parent.width
            height: parent.height
            radius: height / 2
            color: AppTheme.colourText
        }
    }

    handle: Rectangle
    {
        x: root.visualPosition * (root.availableWidth - width)
        y: root.availableHeight / 2 - height / 2
        width: AppMetrics.gapLarge - AppMetrics.gapTight
        height: width
        radius: width / 2
        color: AppTheme.colourPanel
        border.width: AppMetrics.borderWidth * 2
        border.color: AppTheme.colourText
    }

    HoverHandler
    {
        cursorShape: Qt.PointingHandCursor
    }
}
