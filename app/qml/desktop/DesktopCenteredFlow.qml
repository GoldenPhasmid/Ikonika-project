import QtQuick

Item
{
    id: root

    property real spacing: 0
    property bool centred: true
    property var watchedChildren: []

    function arrange()
    {
        if (!root || !root.visible)
        {
            return
        }

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

            let left = root.centred ? Math.round(Math.max(0, (root.width - rowWidth) / 2)) : 0

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
    onVisibleChanged: root.arrange()
    onSpacingChanged: root.arrange()
    onCentredChanged: root.arrange()

    onChildrenChanged:
    {
        const current = []

        for (const child of root.children)
        {
            if (child instanceof Repeater)
            {
                continue
            }

            if (root.watchedChildren.indexOf(child) < 0)
            {
                child.widthChanged.connect(root.arrange)
                child.heightChanged.connect(root.arrange)
                child.visibleChanged.connect(root.arrange)
            }

            current.push(child)
        }

        root.watchedChildren = current
        root.arrange()
        Qt.callLater(root.arrange)
    }
}
