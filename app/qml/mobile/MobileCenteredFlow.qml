import QtQuick

Item
{
    id: root

    property real spacing: 0

    property var watchedItems: []

    function watchChildren()
    {
        for (let index = 0; index < root.children.length; ++index)
        {
            const child = root.children[index]

            if (child instanceof Repeater || root.watchedItems.indexOf(child) >= 0)
            {
                continue
            }

            child.visibleChanged.connect(root.scheduleArrange)
            child.widthChanged.connect(root.scheduleArrange)
            child.heightChanged.connect(root.scheduleArrange)
            root.watchedItems.push(child)
        }

        root.watchedItems = root.watchedItems.filter(Item => Item && Item.parent === root)
    }

    function scheduleArrange()
    {
        Qt.callLater(root.arrange)
    }

    function arrange()
    {
        const items = []

        for (let index = 0; index < root.children.length; ++index)
        {
            const child = root.children[index]

            if (!(child instanceof Repeater) && child.visible)
            {
                items.push(child)
            }
        }

        let top = 0
        let start = 0

        while (start < items.length)
        {
            let end = start
            let rowWidth = 0
            let rowHeight = 0

            while (end < items.length)
            {
                const step = (end > start ? root.spacing : 0) + items[end].width

                if (end > start && rowWidth + step > root.width)
                {
                    break
                }

                rowWidth += step
                rowHeight = Math.max(rowHeight, items[end].height)
                ++end
            }

            let left = Math.round(Math.max(0, (root.width - rowWidth) / 2))

            for (let index = start; index < end; ++index)
            {
                items[index].x = left
                items[index].y = top
                left += items[index].width + root.spacing
            }

            top += rowHeight + root.spacing
            start = end
        }

        root.implicitHeight = items.length > 0 ? top - root.spacing : 0
    }

    onWidthChanged: root.arrange()
    onSpacingChanged: root.arrange()

    onChildrenChanged:
    {
        root.watchChildren()
        root.arrange()
        root.scheduleArrange()
    }

    Component.onDestruction:
    {
        for (const child of root.watchedItems)
        {
            child.visibleChanged.disconnect(root.scheduleArrange)
            child.widthChanged.disconnect(root.scheduleArrange)
            child.heightChanged.disconnect(root.scheduleArrange)
        }

        root.watchedItems = []
    }
}
