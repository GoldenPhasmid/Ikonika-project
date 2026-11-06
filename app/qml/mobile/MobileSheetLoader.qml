import QtQuick

Loader
{
    id: root

    property bool open: false

    active: false

    Component.onCompleted: if (root.open) root.active = true

    onOpenChanged:
    {
        if (!root.open)
        {
            if (root.item)
            {
                root.item.hide()
            }

            return
        }

        if (root.item)
        {
            root.item.present()

            return
        }

        root.active = true
    }

    Connections
    {
        target: root.item

        function onHidden()
        {
            if (!root.open)
            {
                root.active = false
            }
        }
    }
}
