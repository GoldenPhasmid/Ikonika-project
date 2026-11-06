import QtQuick
import QtQuick.Controls.Basic

Item
{
    id: root

    required property string source
    property int initialPage: 0
    property real initialZoom: 0
    property point initialScroll: Qt.point(0, 0)
    property bool cropMargins: true
    property color paperColour: "white"
    property color inkColour: "black"

    signal wordPicked(string word, string sentence)
    signal currentPageChanged(int page)
    signal currentZoomChanged(real zoom)
    signal currentScrollChanged(point scroll)

    property int current: 0
    property real renderScale: 1
    property bool pageDrawn: false

    readonly property int pageIndex: root.current
    readonly property ReaderPdfDocument document: book
    readonly property int pageCount: book.pageCount
    readonly property bool ready: book.ready
    readonly property bool failed: book.failed

    readonly property var sheets: [first, second, third]
    readonly property Item currentSheet: root.sheetAt(root.current)
    readonly property Item previousSheet: root.current > 0 ? root.sheetAt(root.current - 1) : null
    readonly property Item nextSheet: root.current + 1 < root.pageCount ? root.sheetAt(root.current + 1) : null

    readonly property real gap: AppMetrics.readerPageMargin
    readonly property real previousExtent: root.previousSheet && root.previousSheet.inUse ? root.previousSheet.height + root.gap : 0
    readonly property real nextExtent: root.nextSheet && root.nextSheet.inUse ? root.nextSheet.height + root.gap : 0
    readonly property real stackHeight: root.previousExtent + root.currentSheet.height + root.nextExtent
    readonly property real stackTop: Math.max(root.gap, (viewport.height - root.stackHeight) / 2)
    readonly property real currentTop: root.stackTop + root.previousExtent

    readonly property real visibleLeft: root.cropMargins ? Math.max(0, root.currentSheet.cropLeft - AppMetrics.readerCropPadding) : 0
    readonly property real visibleRight: root.cropMargins
        ? Math.min(root.currentSheet.pageSize.width, root.currentSheet.cropRight + AppMetrics.readerCropPadding)
        : root.currentSheet.pageSize.width
    readonly property real fitPoints: Math.max(1, root.visibleRight - root.visibleLeft)
    readonly property real sheetX: Math.max(root.gap, (viewport.contentWidth - root.fitPoints * root.renderScale) / 2)
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
        root.initialPageApplied = false
        root.pageDrawn = false
        root.outlineLoading = false
        root.outlineEntries = []
        root.anchoring = true
        root.current = 0
        root.anchoring = false
        root.clearSelection()
    }

    onCurrentChanged: root.currentPageChanged(root.current)
    onFitPointsChanged: if (root.fitting) Qt.callLater(root.fitWidth)

    onCurrentTopChanged:
    {
        if (!root.anchoring)
        {
            viewport.contentY = root.clamp(viewport.contentY + root.currentTop - root.anchoredTop,
                                           viewport.contentHeight - viewport.height)
        }

        root.anchoredTop = root.currentTop
    }

    onReadyChanged:
    {
        if (!root.ready || root.initialPageApplied)
        {
            return
        }

        root.initialPageApplied = true
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

        root.scrollRestorePending = true
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

    function selectionText()
    {
        return root.sheets.map(sheet => sheet.selectionText).filter(text => text.length > 0).join(" ")
    }

    function selectionGeometry()
    {
        let geometry = null

        for (const sheet of root.sheets)
        {
            const stretch = sheet.selectionStretch

            for (const rectangle of sheet.selectionRectangles)
            {
                const corner = Qt.point(sheet.x + rectangle.x * stretch - viewport.contentX,
                                        sheet.y + rectangle.y * stretch - viewport.contentY)
                const width = rectangle.width * stretch
                const height = rectangle.height * stretch
                const middle = corner.y + height / 2

                if (geometry === null)
                {
                    geometry = { top: corner.y, bottom: corner.y + height, endX: corner.x + width, endY: middle,
                                 left: corner.x, right: corner.x + width }

                    continue
                }

                const sameLine = Math.abs(middle - geometry.endY) <= height / 2

                if ((!sameLine && middle > geometry.endY) || (sameLine && corner.x + width > geometry.endX))
                {
                    geometry.endX = corner.x + width
                    geometry.endY = middle
                }

                geometry.top = Math.min(geometry.top, corner.y)
                geometry.bottom = Math.max(geometry.bottom, corner.y + height)
                geometry.left = Math.min(geometry.left, corner.x)
                geometry.right = Math.max(geometry.right, corner.x + width)
            }
        }

        if (geometry !== null)
        {
            geometry.width = geometry.right - geometry.left
        }

        return geometry
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
        const width = Math.min(viewport.width, AppMetrics.readerPageWidth) - AppMetrics.readerPageMargin * 2

        return Math.max(AppMetrics.readerMinimumZoom, width / root.fitPoints)
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
    }

    function goToPage(target)
    {
        const wanted = Math.max(0, Math.min(root.pageCount - 1, target))

        root.scrollRestorePending = false
        root.clearSelection()
        root.moveTo(wanted, function() { return root.currentTop - root.gap })
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
        if (root.anchoring || !root.ready)
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

    function scrollBy(pixels)
    {
        const limit = viewport.contentHeight - viewport.height

        root.scrollRestorePending = false

        if (limit <= 0)
        {
            root.goToPage(root.current + (pixels > 0 ? 1 : -1))

            return
        }

        viewport.contentY = root.clamp(viewport.contentY + pixels, limit)
        root.followScroll()
    }

    function pagePointUnder(viewportPoint)
    {
        return Qt.point((viewport.contentX + viewportPoint.x - root.sheetX) / root.renderScale,
                        (viewport.contentY + viewportPoint.y - root.currentTop) / root.renderScale)
    }

    function placeAt(scale, pagePoint, viewportPoint)
    {
        root.anchoring = true
        root.renderScale = Math.max(AppMetrics.readerMinimumZoom, Math.min(AppMetrics.readerMaximumZoom, scale))

        viewport.contentX = root.clamp(root.sheetX + pagePoint.x * root.renderScale - viewportPoint.x,
                                       viewport.contentWidth - viewport.width)
        viewport.contentY = root.clamp(root.currentTop + pagePoint.y * root.renderScale - viewportPoint.y,
                                       viewport.contentHeight - viewport.height)

        root.anchoring = false
        root.anchoredTop = root.currentTop
        root.followScroll()
    }

    function zoomAbout(factor, viewportPoint)
    {
        if (!root.ready)
        {
            return
        }

        root.scrollRestorePending = false

        root.placeAt(root.renderScale * factor, root.pagePointUnder(viewportPoint), viewportPoint)
        root.fitting = false
        root.currentZoomChanged(root.renderScale)
    }

    function zoomBy(factor)
    {
        root.zoomAbout(factor, Qt.point(viewport.width / 2, viewport.height / 2))
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
        if (!root.ready || root.scrollRestorePending)
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
        interactive: false
        boundsBehavior: Flickable.StopAtBounds

        contentWidth: Math.max(width, root.fitPoints * root.renderScale + root.gap * 2)
        contentHeight: Math.max(height, root.stackHeight + root.gap * 2)

        onWidthChanged: if (root.fitting) root.fitWidth()
        onContentXChanged: root.reportScroll()
        onContentYChanged: root.reportScroll()

        ScrollBar.horizontal: DesktopScrollBar
        {
            onPressedChanged: if (pressed) root.scrollRestorePending = false
        }

        DesktopReaderPdfSheet
        {
            id: first

            slot: 0
            view: root
            viewport: viewport
        }

        DesktopReaderPdfSheet
        {
            id: second

            slot: 1
            view: root
            viewport: viewport
        }

        DesktopReaderPdfSheet
        {
            id: third

            slot: 2
            view: root
            viewport: viewport
        }
    }

    WheelHandler
    {
        acceptedModifiers: Qt.ControlModifier
        target: null

        onWheel: function(wheel)
        {
            root.zoomAbout(wheel.angleDelta.y > 0 ? AppMetrics.readerZoomStep : 1 / AppMetrics.readerZoomStep,
                           Qt.point(wheel.x, wheel.y))
            wheel.accepted = true
        }
    }

    WheelHandler
    {
        acceptedModifiers: Qt.NoModifier
        target: null

        onWheel: function(wheel)
        {
            root.scrollBy(wheel.angleDelta.y > 0 ? -AppMetrics.readerWheelStep : AppMetrics.readerWheelStep)
            wheel.accepted = true
        }
    }

    WheelHandler
    {
        acceptedModifiers: Qt.ShiftModifier
        target: null

        onWheel: function(wheel)
        {
            viewport.contentX = root.clamp(viewport.contentX + (wheel.angleDelta.y > 0 ? -AppMetrics.readerWheelStep
                                                                                       : AppMetrics.readerWheelStep),
                                           viewport.contentWidth - viewport.width)
            wheel.accepted = true
        }
    }
}
