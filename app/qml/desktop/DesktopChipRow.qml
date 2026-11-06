import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Flickable
{
    id: root

    default property alias chips: chipRow.data
    property alias spacing: chipRow.spacing

    readonly property real ringMargin: AppMetrics.focusRingWidth * 2
    readonly property Item focusedItem: Window.activeFocusItem

    readonly property real chipWidth:
    {
        const widths = []

        for (let index = 0; index < chipRow.children.length; ++index)
        {
            const chip = chipRow.children[index]

            if (chip instanceof DesktopChip && chip.visible)
            {
                widths.push(chip.implicitWidth)
            }
        }

        widths.sort((left, right) => right - left)

        let available = root.width - root.ringMargin * 2 - chipRow.spacing * (widths.length - 1)

        for (let index = 0; index < widths.length; ++index)
        {
            const shared = Math.floor(available / (widths.length - index))

            if (shared >= widths[index])
            {
                return shared
            }

            available -= widths[index]
        }

        return 0
    }

    function reveal(item)
    {
        if (item === null || item.parent !== chipRow)
        {
            return
        }

        const left = item.x
        const right = item.x + item.width + root.ringMargin * 2

        if (left < root.contentX)
        {
            root.contentX = left
        }
        else if (right > root.contentX + root.width)
        {
            root.contentX = right - root.width
        }
    }

    Layout.fillWidth: true
    Layout.leftMargin: -root.ringMargin
    Layout.rightMargin: -root.ringMargin
    Layout.topMargin: -root.ringMargin
    Layout.bottomMargin: -root.ringMargin
    implicitHeight: chipRow.height + root.ringMargin * 2

    contentWidth: chipRow.width + root.ringMargin * 2
    contentHeight: height
    flickableDirection: Flickable.HorizontalFlick
    boundsBehavior: Flickable.StopAtBounds
    interactive: root.contentWidth > root.width
    clip: true

    onFocusedItemChanged: root.reveal(root.focusedItem)

    WheelHandler
    {
        enabled: root.interactive
        orientation: Qt.Vertical

        onWheel: function(event)
        {
            const furthest = root.contentWidth - root.width

            root.contentX = Math.max(0, Math.min(furthest, root.contentX - event.angleDelta.y))
        }
    }

    Row
    {
        id: chipRow

        x: root.ringMargin
        y: root.ringMargin
    }
}
