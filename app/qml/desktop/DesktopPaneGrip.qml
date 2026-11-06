import QtQuick

Item
{
    id: root

    property string widthKey: ""
    property int defaultWidth: AppPaneWidths.minimumWidth
    property bool onLeft: false

    property real draggedWidth: -1

    readonly property real paneWidth: root.draggedWidth >= 0
                                      ? root.draggedWidth
                                      : (AppPaneWidths.revision, AppPaneWidths.width(root.widthKey, root.defaultWidth))

    anchors.top: parent ? parent.top : undefined
    anchors.bottom: parent ? parent.bottom : undefined
    x: root.onLeft ? -width / 2 : (parent ? parent.width - width / 2 : 0)
    z: 2
    width: AppMetrics.gapSmall

    Accessible.ignored: true

    HoverHandler
    {
        cursorShape: Qt.SplitHCursor
    }

    DragHandler
    {
        id: drag

        property real startWidth: 0

        target: null
        yAxis.enabled: false
        cursorShape: Qt.SplitHCursor
        grabPermissions: PointerHandler.CanTakeOverFromAnything

        onActiveChanged:
        {
            if (drag.active)
            {
                drag.startWidth = root.paneWidth

                return
            }

            if (root.draggedWidth >= 0)
            {
                AppPaneWidths.setWidth(root.widthKey, Math.round(root.draggedWidth))
            }

            root.draggedWidth = -1
        }

        onActiveTranslationChanged:
        {
            if (!drag.active)
            {
                return
            }

            const pane = root.parent
            const room = pane && pane.parent ? pane.parent.width / 2 : AppPaneWidths.maximumWidth
            const limit = Math.min(AppPaneWidths.maximumWidth, room)
            const travel = drag.centroid.scenePosition.x - drag.centroid.scenePressPosition.x
            const delta = root.onLeft ? -travel : travel

            root.draggedWidth = Math.max(AppPaneWidths.minimumWidth, Math.min(limit, drag.startWidth + delta))
        }
    }
}
