import QtQuick

DragHandler
{
    id: root

    required property TileDrag overlay
    required property int tileIndex
    required property string tileText
    required property bool fromAnswer

    target: null
    grabPermissions: PointerHandler.CanTakeOverFromAnything

    onActiveChanged:
    {
        if (root.active)
        {
            root.overlay.begin(root.parent, root.tileIndex, root.tileText, root.fromAnswer,
                               root.centroid.pressPosition, root.centroid.scenePosition)

            return
        }

        root.overlay.finish()
    }

    onCentroidChanged: if (root.active) root.overlay.move(root.centroid.scenePosition)
}
