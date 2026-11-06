import QtQuick

Item
{
    id: root

    required property string source
    property int initialPage: 0
    property real initialZoom: 0
    property point initialScroll: Qt.point(0, 0)
    property bool cropMargins: false
    property color paperColour: "white"
    property color inkColour: "black"

    signal wordPicked(string word, string sentence)
    signal doubleTapped()
    signal blankTapped()
    signal currentPageChanged(int page)
    signal currentZoomChanged(real zoom)
    signal currentScrollChanged(point scroll)

    property int current: 0
    property real renderScale: 1
    property bool presented: false

    readonly property int pageIndex: root.current
    readonly property ReaderPdfDocument document: book
    readonly property int pageCount: book.pageCount
    readonly property bool ready: book.ready
    readonly property bool failed: book.failed

    readonly property var sheets: [first, second, third]
    readonly property Item currentSheet: root.sheetAt(root.current)
    readonly property Item previousSheet: root.current > 0 ? root.sheetAt(root.current - 1) : null
    readonly property Item nextSheet: root.current + 1 < root.pageCount ? root.sheetAt(root.current + 1) : null

    readonly property int pageMargin: 1
    readonly property real minimumScale: 0.4
    readonly property real maximumScale: 6
    readonly property real cropPadding: 8
    readonly property real gap: root.pageMargin
    readonly property real previousExtent: root.previousSheet && root.previousSheet.inUse ? root.previousSheet.height + root.gap : 0
    readonly property real nextExtent: root.nextSheet && root.nextSheet.inUse ? root.nextSheet.height + root.gap : 0
    readonly property real stackHeight: root.previousExtent + root.currentSheet.height + root.nextExtent
    readonly property real stackTop: Math.max(root.gap, (viewport.height - root.stackHeight) / 2)
    readonly property real currentTop: root.stackTop + root.previousExtent

    readonly property real visibleLeft: root.cropMargins ? Math.max(0, root.currentSheet.cropLeft - root.cropPadding) : 0
    readonly property real visibleRight: root.cropMargins
        ? Math.min(root.currentSheet.pageSize.width, root.currentSheet.cropRight + root.cropPadding)
        : root.currentSheet.pageSize.width
    readonly property real fitPoints: Math.max(1, root.visibleRight - root.visibleLeft)
    readonly property real sheetX: Math.max(0, (viewport.contentWidth - root.fitPoints * root.renderScale) / 2)
        - root.visibleLeft * root.renderScale

    property bool fitting: true
    property bool initialPageApplied: false
    property bool scrollRestorePending: false
    property bool anchoring: false
    property real anchoredTop: 0
    property bool outlineLoading: false
    property var outlineEntries: []

    onSourceChanged:
    {
        root.presented = false
        for (const sheet of root.sheets)
        {
            sheet.hasRendered = false
        }
        root.initialPageApplied = false
        root.outlineLoading = false
        root.outlineEntries = []
        root.anchoring = true
        root.current = 0
        root.anchoring = false
        root.clearSelection()
    }

    onCurrentChanged: root.currentPageChanged(root.current)
    onCropMarginsChanged: if (root.fitting) root.fitWidth()
    onFitPointsChanged: Qt.callLater(function()
    {
        if (root.fitting)
        {
            root.renderScale = root.fitScale()
        }
    })

    onCurrentTopChanged:
    {
        if (!root.anchoring)
        {
            viewport.contentY = root.clamp(viewport.contentY + root.currentTop - root.anchoredTop,
                                           viewport.contentHeight - viewport.height)
        }

        root.anchoredTop = root.currentTop
    }

    onReadyChanged: Qt.callLater(root.restorePosition)

    function finishOpening()
    {
        if (root.ready && root.initialPageApplied && root.currentSheet.hasRendered)
        {
            root.presented = true
        }
    }

    function restorePosition()
    {
        if (!root.ready || root.initialPageApplied)
        {
            return
        }

        root.initialPageApplied = true
        root.scrollRestorePending = true
        root.anchoring = true
        root.current = Math.max(0, Math.min(root.pageCount - 1, root.initialPage))
        root.anchoring = false

        if (root.initialZoom > 0)
        {
            root.renderScale = root.initialZoom
            root.fitting = false
        }
        else
        {
            root.fitWidth()
        }

        Qt.callLater(root.applyInitialScroll)
    }

    function sheetAt(pageNumber)
    {
        return root.sheets[((pageNumber % 3) + 3) % 3]
    }

    function clearSelection()
    {
        for (const sheet of root.sheets)
        {
            sheet.clearSelection()
        }
    }

    function requestOutline()
    {
        root.outlineLoading = true
        root.outlineEntries = []
        book.requestOutline()
    }

    function clamp(value, limit)
    {
        return Math.max(0, Math.min(Math.max(0, limit), value))
    }

    function fitScale()
    {
        const width = viewport.width

        return Math.max(root.minimumScale, width / root.fitPoints)
    }

    function fitWidth()
    {
        if (!root.ready || viewport.width <= 0)
        {
            return
        }

        root.placeAt(root.fitScale(), root.pagePointUnder(Qt.point(0, 0)), Qt.point(0, 0))
        viewport.contentX = 0
        root.fitting = true
        root.currentZoomChanged(0)
    }

    function moveTo(target, contentY)
    {
        root.anchoring = true
        root.current = target
        viewport.contentY = root.clamp(contentY(), viewport.contentHeight - viewport.height)
        root.anchoring = false
        root.anchoredTop = root.currentTop
        root.reportScroll()
    }

    function goToPage(target)
    {
        const wanted = Math.max(0, Math.min(root.pageCount - 1, target))

        root.scrollRestorePending = false
        root.clearSelection()
        viewport.cancelFlick()
        root.moveTo(wanted, function() { return root.currentTop - root.gap })
        viewport.contentX = 0
        root.reportScroll()
    }

    function shiftTo(target)
    {
        const sheet = root.sheetAt(target)
        const before = sheet.y
        const scrolled = viewport.contentY

        root.moveTo(target, function() { return scrolled + sheet.y - before })
    }

    function followScroll()
    {
        if (root.anchoring || pinch.active || !root.ready || !root.initialPageApplied || root.scrollRestorePending)
        {
            return
        }

        for (let step = 0; step < 3; ++step)
        {
            const centre = viewport.contentY + viewport.height / 2

            if (root.nextSheet && root.nextSheet.inUse && centre >= root.nextSheet.y - root.gap / 2)
            {
                root.shiftTo(root.current + 1)

                continue
            }

            if (root.previousSheet && root.previousSheet.inUse && centre < root.currentTop - root.gap / 2)
            {
                root.shiftTo(root.current - 1)

                continue
            }

            return
        }
    }

    function pagePointUnder(viewportPoint)
    {
        return Qt.point((viewport.contentX + viewportPoint.x - root.sheetX) / root.renderScale,
                        (viewport.contentY + viewportPoint.y - root.currentTop) / root.renderScale)
    }

    function placeAt(scale, pagePoint, viewportPoint)
    {
        root.anchoring = true
        root.renderScale = Math.max(root.fitScale(), Math.min(root.maximumScale, scale))

        viewport.contentX = root.clamp(root.sheetX + pagePoint.x * root.renderScale - viewportPoint.x,
                                       viewport.contentWidth - viewport.width)
        viewport.contentY = root.clamp(root.currentTop + pagePoint.y * root.renderScale - viewportPoint.y,
                                       viewport.contentHeight - viewport.height)

        root.anchoring = false
        root.anchoredTop = root.currentTop
        root.followScroll()
    }

    function viewportPointOf(scenePoint)
    {
        return viewport.mapFromItem(null, scenePoint.x, scenePoint.y)
    }

    function holdsPage(spot)
    {
        return root.sheets.some(sheet => sheet.inUse && spot.x >= sheet.x
            && spot.x < sheet.x + sheet.width && spot.y >= sheet.y && spot.y < sheet.y + sheet.height)
    }

    function toggleZoom(scenePoint)
    {
        if (!root.ready || root.currentSheet.pageSize.width <= 0)
        {
            return
        }

        root.scrollRestorePending = false

        const fit = root.fitScale()
        const zoomed = Math.min(root.maximumScale, fit * 2.2)
        const target = root.renderScale < fit * 1.3 ? zoomed : fit
        const viewportPoint = root.viewportPointOf(scenePoint)

        root.placeAt(target, root.pagePointUnder(viewportPoint), viewportPoint)
        root.fitting = target === fit
        root.currentZoomChanged(root.fitting ? 0 : root.renderScale)
        root.reportScroll()
    }

    function applyInitialScroll()
    {
        if (!root.scrollRestorePending)
        {
            return
        }

        root.anchoring = true
        viewport.contentX = root.clamp(root.initialScroll.x * root.renderScale, viewport.contentWidth - viewport.width)
        viewport.contentY = root.clamp(root.currentTop - root.gap + root.initialScroll.y * root.renderScale,
                                       viewport.contentHeight - viewport.height)
        root.anchoring = false
        root.anchoredTop = root.currentTop
    }

    function reportScroll()
    {
        if (!root.ready || root.scrollRestorePending || root.anchoring)
        {
            return
        }

        root.currentScrollChanged(Qt.point(viewport.contentX / root.renderScale,
                                           (viewport.contentY - root.currentTop + root.gap) / root.renderScale))
    }

    ReaderPdfDocument
    {
        id: book

        source: root.source

        onOutlineReady: function(entries)
        {
            root.outlineEntries = entries
            root.outlineLoading = false
        }
    }

    Flickable
    {
        id: viewport

        anchors.fill: parent
        clip: true
        flickableDirection: Flickable.HorizontalAndVerticalFlick
        boundsBehavior: Flickable.StopAtBounds

        contentWidth: Math.max(width, root.fitPoints * root.renderScale)
        contentHeight: Math.max(height, root.stackHeight + root.gap * 2)

        onWidthChanged: if (root.fitting) root.fitWidth()
        onContentXChanged: root.reportScroll()
        onContentYChanged:
        {
            if (!root.anchoring)
            {
                Qt.callLater(root.followScroll)
            }
            root.reportScroll()
        }
        onMovementStarted: root.scrollRestorePending = false
        onMovementEnded: root.reportScroll()

        MobileReaderPdfSheet
        {
            id: first

            slot: 0
            view: root
            viewport: viewport
        }

        MobileReaderPdfSheet
        {
            id: second

            slot: 1
            view: root
            viewport: viewport
        }

        MobileReaderPdfSheet
        {
            id: third

            slot: 2
            view: root
            viewport: viewport
        }

        TapHandler
        {
            onSingleTapped: function(eventPoint)
            {
                if (!root.holdsPage(eventPoint.position))
                {
                    root.blankTapped()
                }
            }
        }

        PinchHandler
        {
            id: pinch

            target: null
            grabPermissions: PointerHandler.CanTakeOverFromAnything

            property real gestureStartScale: 1
            property point gesturePagePoint

            function follow()
            {
                if (!pinch.active)
                {
                    return
                }

                root.placeAt(pinch.gestureStartScale * pinch.activeScale, pinch.gesturePagePoint,
                             root.viewportPointOf(pinch.centroid.scenePosition))
            }

            onActiveChanged:
            {
                if (pinch.active)
                {
                    root.scrollRestorePending = false
                    root.fitting = false
                    viewport.cancelFlick()
                    viewport.interactive = false
                    pinch.gestureStartScale = root.renderScale
                    pinch.gesturePagePoint = root.pagePointUnder(
                        root.viewportPointOf(pinch.centroid.scenePosition))

                    return
                }

                viewport.interactive = true
                viewport.returnToBounds()
                root.fitting = root.renderScale <= root.fitScale()
                root.currentZoomChanged(root.fitting ? 0 : root.renderScale)
                root.followScroll()
                root.reportScroll()
            }

            onActiveScaleChanged: pinch.follow()
            onCentroidChanged: pinch.follow()
        }
    }
}
