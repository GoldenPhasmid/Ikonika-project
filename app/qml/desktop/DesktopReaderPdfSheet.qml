import QtQuick

ReaderPdfPage
{
    id: root

    required property int slot
    required property Item view
    required property Flickable viewport

    readonly property int slotPage: root.view.current - 1 + (((root.slot - (root.view.current - 1)) % 3) + 3) % 3
    readonly property bool inUse: root.ready && root.slotPage >= 0 && root.slotPage < root.pageCount

    property var selectionRectangles: []
    property string selectionText: ""
    property string selectionSentence: ""
    property real selectionScale: 1

    readonly property real selectionStretch: root.renderScale / root.selectionScale

    readonly property var readRange:
    {
        if (!root.inUse)
        {
            return null
        }

        for (const range of root.view.readRanges)
        {
            if (range.page === root.page)
            {
                return range
            }
        }

        return null
    }

    readonly property var readRectangles: root.readRange !== null ? root.textRectangles(root.readRange.start, root.readRange.length) : []

    function clearSelection()
    {
        root.selectionRectangles = []
        root.selectionText = ""
    }

    function showSelection(found)
    {
        root.view.pickedPlace = found.text.length > 0 ? { page: found.page, index: found.index } : null
        root.selectionScale = root.renderScale
        root.selectionRectangles = found.rectangles
        root.selectionText = found.text
        root.selectionSentence = found.sentence || ""
    }

    function followLinkAt(position)
    {
        const link = root.internalLinkAt(position)

        if (link.page === undefined)
        {
            return false
        }

        root.view.goToPage(link.page, link.location)

        return true
    }

    document: root.view.document
    paperColour: root.view.paperColour
    inkColour: root.view.inkColour
    renderScale: root.view.renderScale
    prefetch: false
    visible: root.inUse

    x: root.view.sheetX
    y: root === root.view.previousSheet ? root.view.stackTop
        : root === root.view.currentSheet ? root.view.currentTop
        : root.view.currentTop + root.view.currentSheet.height + root.view.gap

    width: root.pageSize.width * root.renderScale
    height: root.pageSize.height * root.renderScale

    visibleArea: root.inUse
        ? Qt.rect(root.viewport.contentX - root.x, root.viewport.contentY - root.y, root.viewport.width, root.viewport.height)
        : Qt.rect(0, 0, 0, 0)

    onPageChanged: root.clearSelection()
    onRendered: if (root === root.view.currentSheet) root.view.pageDrawn = true

    Binding on page
    {
        when: root.inUse
        value: root.slotPage
        restoreMode: Binding.RestoreNone
    }

    Rectangle
    {
        anchors.fill: parent
        anchors.margins: -AppMetrics.borderWidth
        z: -1

        color: root.paperColour
        border.width: AppMetrics.borderWidth
        border.color: AppTheme.colourBorder
    }

    DragHandler
    {
        id: selectionDrag

        acceptedDevices: PointerDevice.Mouse | PointerDevice.Stylus
        target: null
        cursorShape: Qt.IBeamCursor

        onActiveChanged:
        {
            root.view.selecting = selectionDrag.active

            if (selectionDrag.active)
            {
                root.view.clearSelection()

                return
            }

            const picked = root.selectionText.trim()

            if (picked.length > 0)
            {
                root.view.wordPicked(picked, root.selectionSentence)
            }
        }

        onCentroidChanged:
        {
            if (selectionDrag.active)
            {
                root.showSelection(root.selectionBetween(centroid.pressPosition, centroid.position))
            }
        }
    }

    TapHandler
    {
        id: mouseTap

        acceptedDevices: PointerDevice.Mouse | PointerDevice.Stylus

        onTapped: function(eventPoint)
        {
            if (root.followLinkAt(eventPoint.position))
            {
                return
            }

            const found = root.selectWordAt(eventPoint.position)

            root.view.clearSelection()
            root.showSelection(found)

            if (mouseTap.tapCount === 2 && found.text.length > 0 && root.view.narrated && root.view.readAloud.available)
            {
                root.view.readAloudJumpRequested()

                return
            }

            if (found.text.length > 0)
            {
                root.view.wordPicked(found.text, root.selectionSentence)
                root.view.translationWanted()
            }
        }
    }

    TapHandler
    {
        id: touchTap

        acceptedDevices: PointerDevice.TouchScreen
        longPressThreshold: 0.4

        property bool selecting: false
        property point anchorPoint
        readonly property point livePoint: touchTap.point.position

        onSingleTapped: function(eventPoint)
        {
            if (root.followLinkAt(eventPoint.position))
            {
                return
            }

            const found = root.selectWordAt(eventPoint.position)

            if (found.text.length === 0)
            {
                root.view.blankTapped()

                return
            }

            root.view.clearSelection()
            root.showSelection(found)
            root.view.wordPicked(found.text, root.selectionSentence)
            root.view.translationWanted()
        }

        onDoubleTapped: function(eventPoint)
        {
            root.view.doubleTapped()
            root.view.toggleZoom(eventPoint.scenePosition)
        }

        onLongPressed:
        {
            touchTap.selecting = true
            touchTap.anchorPoint = touchTap.point.position
            root.viewport.interactive = false
            root.view.selecting = true

            const found = root.selectWordAt(touchTap.anchorPoint)

            root.view.clearSelection()
            root.showSelection(found)
        }

        onLivePointChanged:
        {
            if (!touchTap.selecting)
            {
                return
            }

            const found = root.selectionBetween(touchTap.anchorPoint, touchTap.livePoint)

            if (found.text.length === 0)
            {
                return
            }

            root.view.clearSelection()
            root.showSelection(found)
        }

        onPressedChanged:
        {
            if (touchTap.pressed || !touchTap.selecting)
            {
                return
            }

            touchTap.selecting = false
            root.viewport.interactive = true
            root.view.selecting = false

            const picked = root.selectionText.trim()

            if (picked.length > 0)
            {
                root.view.wordPicked(picked, root.selectionSentence)
            }
        }
    }

    HoverHandler
    {
        id: pageHover

        cursorShape: pageHover.hovered && root.inUse && root.page === root.slotPage
            && root.internalLinkAt(pageHover.point.position).page !== undefined ? Qt.PointingHandCursor : Qt.IBeamCursor
    }

    Repeater
    {
        model: root.readRectangles

        delegate: Rectangle
        {
            required property var modelData

            x: modelData.x * root.renderScale
            y: modelData.y * root.renderScale
            width: modelData.width * root.renderScale
            height: modelData.height * root.renderScale

            color: AppTheme.faded(root.inkColour, AppTheme.alphaSoft)
        }
    }

    Repeater
    {
        model: root.selectionRectangles

        delegate: Rectangle
        {
            required property var modelData

            x: modelData.x * root.selectionStretch
            y: modelData.y * root.selectionStretch
            width: modelData.width * root.selectionStretch
            height: modelData.height * root.selectionStretch

            color: AppTheme.faded(AppTheme.colourAccent, AppTheme.alphaHalf)
        }
    }
}
