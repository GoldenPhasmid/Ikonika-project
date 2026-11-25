import QtQuick

ReaderPdfPage
{
    id: root

    required property int slot
    required property Item view
    required property Flickable viewport

    readonly property int slotPage: root.view.current - 1 + (((root.slot - (root.view.current - 1)) % 3) + 3) % 3
    readonly property bool inUse: root.ready && root.slotPage >= 0 && root.slotPage < root.pageCount
    readonly property real visibleLeft: root.view.cropMargins ? Math.max(0, root.cropLeft - root.view.cropPadding) : 0
    readonly property real visibleRight: root.view.cropMargins
        ? Math.min(root.pageSize.width, root.cropRight + root.view.cropPadding) : root.pageSize.width
    readonly property real fitPoints: Math.max(1, root.visibleRight - root.visibleLeft)

    property var selectionRectangles: []
    property string selectionText: ""
    property string selectionSentence: ""
    property real selectionScale: 1
    property bool hasRendered: false

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

    readonly property real selectionStretch: root.renderScale / root.selectionScale

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

    document: root.view.document
    paperColour: root.view.paperColour
    inkColour: root.view.inkColour
    renderScale: root.view.fitting
        ? Math.max(root.view.minimumScale, root.viewport.width / root.fitPoints) : root.view.renderScale
    prefetch: false
    visible: root.inUse

    x: Math.max(0, (root.viewport.contentWidth - root.fitPoints * root.renderScale) / 2)
        - root.visibleLeft * root.renderScale
    y: root === root.view.previousSheet ? root.view.stackTop
        : root === root.view.currentSheet ? root.view.currentTop
        : root.view.currentTop + root.view.currentSheet.height + root.view.gap

    width: root.pageSize.width * root.renderScale
    height: root.pageSize.height * root.renderScale

    visibleArea: root.inUse
        ? Qt.rect(root.viewport.contentX - root.x, root.viewport.contentY - root.y, root.viewport.width, root.viewport.height)
        : Qt.rect(0, 0, 0, 0)

    onPageChanged:
    {
        root.hasRendered = false
        root.clearSelection()

        if (root === root.view.currentSheet && root.view.scrollRestorePending)
        {
            Qt.callLater(root.view.applyInitialScroll)
        }
    }

    onRendered:
    {
        root.hasRendered = true
        Qt.callLater(root.view.finishOpening)
    }

    Binding on page
    {
        when: root.inUse
        value: root.slotPage
        restoreMode: Binding.RestoreNone
    }

    Rectangle
    {
        anchors.fill: parent
        anchors.margins: -1
        z: -1

        color: root.paperColour
        border.width: 1
        border.color: AppTheme.colourBorder
    }

    TapHandler
    {
        id: tapHandler

        exclusiveSignals: TapHandler.SingleTap | TapHandler.DoubleTap
        longPressThreshold: 0.4

        property bool selecting: false
        property point anchorPoint
        readonly property point livePoint: point.position

        onSingleTapped: function(eventPoint)
        {
            const position = root.mapFromItem(null, eventPoint.scenePosition)
            const link = root.internalLinkAt(position)

            if (link.page !== undefined)
            {
                root.view.goToPage(link.page, link.location)

                return
            }

            const found = root.selectWordAt(position)

            if (found.text.length === 0)
            {
                root.view.blankTapped()

                return
            }

            root.view.clearSelection()
            root.showSelection(found)
            root.view.wordPicked(found.text, root.selectionSentence)
        }

        onDoubleTapped: function(eventPoint)
        {
            const found = root.selectWordAt(root.mapFromItem(null, eventPoint.scenePosition))

            if (root.view.narrated && found.text.length > 0)
            {
                root.view.readAloudRequested({ page: found.page, index: found.index })

                return
            }

            root.view.doubleTapped()
            root.view.toggleZoom(eventPoint.scenePosition)
        }

        onLongPressed:
        {
            tapHandler.selecting = true
            tapHandler.anchorPoint = tapHandler.point.position
            root.viewport.interactive = false

            const found = root.selectWordAt(tapHandler.anchorPoint)
            root.view.clearSelection()
            root.showSelection(found)
        }

        onLivePointChanged:
        {
            if (!tapHandler.selecting)
            {
                return
            }

            const found = root.selectionBetween(tapHandler.anchorPoint, tapHandler.livePoint)

            if (found.text.length === 0)
            {
                return
            }

            root.view.clearSelection()
            root.showSelection(found)
        }

        onPressedChanged:
        {
            if (tapHandler.pressed || !tapHandler.selecting)
            {
                return
            }

            tapHandler.selecting = false
            root.viewport.interactive = true

            if (root.selectionText.length > 0)
            {
                root.view.wordPicked(root.selectionText, root.selectionSentence)
            }
        }
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

            color: AppTheme.colourAccent
            opacity: 0.35
        }
    }
}
