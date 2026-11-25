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
    property real startPadding: 0
    property real endPadding: 0
    property real bottomChrome: 0
    property color paperColour: "white"
    property color inkColour: "black"
    property ReaderReadAloud readAloud: null
    property ReaderPdfNarration narration: null
    property real readAloudInset: 0
    property real topChrome: 0

    signal wordPicked(string word, string sentence)
    signal translationWanted()
    signal readAloudJumpRequested()
    signal currentPageChanged(int page)
    signal currentZoomChanged(real zoom)
    signal currentScrollChanged(point scroll)
    signal doubleTapped()
    signal blankTapped()

    property int current: 0
    property real renderScale: 1
    property bool pageDrawn: false
    property bool selecting: false
    property var pickedPlace: null
    property bool followingReadAloud: false
    property bool readingAway: false
    readonly property bool autoscrolling: autoscroll.active

    readonly property int pageIndex: root.current
    readonly property ReaderPdfDocument document: book
    readonly property int pageCount: book.pageCount
    readonly property bool ready: book.ready
    readonly property bool failed: book.failed
    readonly property bool narrated: root.readAloud !== null && root.narration !== null && root.narration.available
    readonly property var readRanges: root.narrated && root.readAloud.started
        ? root.narration.pageRanges(root.readAloud.paragraph, root.readAloud.sentenceStart, root.readAloud.sentenceEnd)
        : []

    readonly property var sheets: [first, second, third]
    readonly property Item currentSheet: root.sheetAt(root.current)
    readonly property Item previousSheet: root.current > 0 ? root.sheetAt(root.current - 1) : null
    readonly property Item nextSheet: root.current + 1 < root.pageCount ? root.sheetAt(root.current + 1) : null

    readonly property real gap: AppMetrics.readerPageMargin
    readonly property real previousExtent: root.previousSheet && root.previousSheet.inUse ? root.previousSheet.height + root.gap : 0
    readonly property real nextExtent: root.nextSheet && root.nextSheet.inUse ? root.nextSheet.height + root.gap : 0
    readonly property real stackHeight: root.previousExtent + root.currentSheet.height + root.nextExtent
    readonly property real topInset: root.previousSheet === null ? root.startPadding : 0
    readonly property real bottomInset: root.nextSheet === null ? root.endPadding : 0
    readonly property real stackTop: Math.max(root.gap + root.topInset, (viewport.height - root.stackHeight) / 2)
    readonly property real currentTop: root.stackTop + root.previousExtent

    readonly property real visibleLeft: root.cropMargins ? Math.max(0, root.currentSheet.cropLeft - AppMetrics.readerCropPadding) : 0
    readonly property real visibleRight: root.cropMargins
        ? Math.min(root.currentSheet.pageSize.width, root.currentSheet.cropRight + AppMetrics.readerCropPadding)
        : root.currentSheet.pageSize.width
    readonly property real fitPoints: Math.max(1, root.visibleRight - root.visibleLeft)
    readonly property real sheetX: Math.max(root.gap, (viewport.contentWidth - root.fitPoints * root.renderScale) / 2)
        - root.visibleLeft * root.renderScale

    readonly property real pageStride: root.currentSheet.height + root.gap
    readonly property bool measured: root.ready && root.pageCount > 0 && root.pageStride > 0
    readonly property real documentSize: root.measured ? Math.min(1, viewport.height / root.pageStride / root.pageCount) : 1
    readonly property real documentPosition:
    {
        if (!root.measured)
        {
            return 0
        }

        const pageTop = root.currentTop

        return Math.max(0, (root.current + (viewport.contentY - pageTop + root.gap) / root.pageStride) / root.pageCount)
    }

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
        root.pickedPlace = null

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

    function readingSpan()
    {
        let span = null

        for (const range of root.readRanges)
        {
            const sheet = root.sheetAt(range.page)

            if (!sheet.inUse || sheet.page !== range.page)
            {
                continue
            }

            for (const rectangle of sheet.textRectangles(range.start, range.length))
            {
                const top = sheet.y + rectangle.y * root.renderScale
                const bottom = sheet.y + (rectangle.y + rectangle.height) * root.renderScale

                span = span === null ? { top: top, bottom: bottom }
                                     : { top: Math.min(span.top, top), bottom: Math.max(span.bottom, bottom) }
            }
        }

        return span
    }

    function readingVisibleTop()
    {
        return viewport.contentY + root.topChrome
    }

    function readingVisibleBottom()
    {
        return viewport.contentY + viewport.height - root.readAloudInset
    }

    function isReadingShown()
    {
        const span = root.readingSpan()

        return span !== null && span.bottom > root.readingVisibleTop() && span.top < root.readingVisibleBottom()
    }

    function playReadAloud()
    {
        if (!root.narrated)
        {
            return
        }

        root.followingReadAloud = true

        if (root.readAloud.started && root.isReadingShown())
        {
            root.readAloud.resume()

            return
        }

        root.narration.prepare(root.current)

        if (!root.readAloud.started && root.readAloud.savedParagraph >= 0
            && root.narration.pageOf(root.readAloud.savedParagraph, root.readAloud.savedCharacter) === root.current)
        {
            root.readAloud.startAt(root.readAloud.savedParagraph, root.readAloud.savedCharacter)

            return
        }

        const place = root.narration.unitPlace(root.current, 0)

        if (place.paragraph === undefined)
        {
            return
        }

        root.readAloud.startAt(place.paragraph, place.character)
    }

    function readFrom(place)
    {
        if (!root.narrated || place === null)
        {
            return
        }

        root.narration.prepare(place.page)

        const found = root.narration.unitPlace(place.page, place.index)

        if (found.paragraph === undefined)
        {
            return
        }

        root.clearSelection()
        root.followingReadAloud = true
        root.readAloud.startAt(found.paragraph, found.character)
    }

    function leaveReadAloud()
    {
        root.followingReadAloud = false
    }

    function showReading()
    {
        root.followingReadAloud = true
        root.followReadAloud(true)
    }

    function updateReadingAway()
    {
        root.readingAway = root.narrated && root.readAloud.started && !root.followingReadAloud && !root.isReadingShown()
    }

    function followReadAloud(force)
    {
        if (!root.narrated || !root.readAloud.started || !root.followingReadAloud || (!force && !root.readAloud.playing)
            || root.readRanges.length === 0)
        {
            return
        }

        let span = root.readingSpan()

        if (span !== null && span.top >= root.readingVisibleTop() && span.bottom <= root.readingVisibleBottom())
        {
            return
        }

        if (span === null)
        {
            root.moveTo(root.readRanges[0].page, function() { return root.currentTop - root.gap })
            span = root.readingSpan()

            if (span === null)
            {
                return
            }
        }

        root.scrollRestorePending = false
        root.anchoring = true
        viewport.contentY = root.clamp(span.top - root.topChrome - AppMetrics.gapLarge, viewport.contentHeight - viewport.height)
        root.anchoring = false
        root.anchoredTop = root.currentTop
        root.followScroll()
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

    function goToPage(target, location)
    {
        const wanted = Math.max(0, Math.min(root.pageCount - 1, target))

        root.leaveReadAloud()
        root.scrollRestorePending = false
        root.clearSelection()
        root.moveTo(wanted, function()
        {
            const offset = location === undefined ? -root.gap : Math.max(-root.gap,
                location.y * root.currentSheet.renderScale - root.topChrome)
            return root.currentTop + offset
        })

        if (location !== undefined)
        {
            viewport.cancelFlick()
            viewport.contentX = root.clamp(root.currentSheet.x + location.x * root.currentSheet.renderScale - root.gap,
                                           viewport.contentWidth - viewport.width)
            root.reportScroll()
        }
    }

    function scrollToDocumentPosition(position)
    {
        if (!root.measured)
        {
            return
        }

        const target = Math.max(0, Math.min(root.pageCount, position * root.pageCount))
        const page = Math.min(root.pageCount - 1, Math.floor(target))

        if (page !== root.current)
        {
            root.goToPage(page)
        }

        root.leaveReadAloud()
        root.scrollRestorePending = false
        viewport.contentY = root.clamp(root.currentTop - root.gap + (target - page) * root.pageStride,
                                       viewport.contentHeight - viewport.height)
        root.followScroll()
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

        root.leaveReadAloud()
        root.scrollRestorePending = false

        if (limit <= 0)
        {
            root.goToPage(root.current + (pixels > 0 ? 1 : -1))

            return
        }

        viewport.contentY = root.clamp(viewport.contentY + pixels, limit)
        root.followScroll()
    }

    function stopAutoscroll()
    {
        autoscroll.stop()
    }

    function autoscrollBy(pixels)
    {
        const limit = viewport.contentHeight - viewport.height

        if (limit <= 0)
        {
            return
        }

        root.leaveReadAloud()
        root.scrollRestorePending = false
        viewport.contentY = root.clamp(viewport.contentY + pixels, limit)
        root.followScroll()
    }

    function pagePointUnder(viewportPoint)
    {
        return Qt.point((viewport.contentX + viewportPoint.x - root.sheetX) / root.renderScale,
                        (viewport.contentY + viewportPoint.y - root.currentTop) / root.renderScale)
    }

    function viewportPointOf(scenePoint)
    {
        return viewport.mapFromItem(null, scenePoint.x, scenePoint.y)
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
        const zoomed = Math.min(AppMetrics.readerMaximumZoom, fit * 2.2)
        const target = root.renderScale < fit * 1.3 ? zoomed : fit
        const viewportPoint = root.viewportPointOf(scenePoint)

        root.placeAt(target, root.pagePointUnder(viewportPoint), viewportPoint)
        root.fitting = target === fit
        root.currentZoomChanged(root.fitting ? 0 : root.renderScale)
        root.reportScroll()
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

    Connections
    {
        target: root.readAloud

        function onStateChanged()
        {
            Qt.callLater(root.followReadAloud, false)
            Qt.callLater(root.updateReadingAway)
        }
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
        acceptedButtons: Qt.NoButton
        flickableDirection: Flickable.HorizontalAndVerticalFlick
        boundsBehavior: Flickable.StopAtBounds

        contentWidth: Math.max(width, root.fitPoints * root.renderScale + root.gap * 2)
        contentHeight: Math.max(height, root.topInset + root.stackHeight + root.bottomInset + root.gap * 2)

        onWidthChanged: if (root.fitting) root.fitWidth()
        onContentXChanged: root.reportScroll()
        onContentYChanged:
        {
            if (!root.anchoring)
            {
                Qt.callLater(root.followScroll)
            }

            root.reportScroll()
            root.updateReadingAway()
        }
        onMovementStarted:
        {
            root.scrollRestorePending = false
            root.leaveReadAloud()
        }

        ScrollBar.horizontal: DesktopScrollBar
        {
            parent: viewport.parent
            anchors.left: viewport.left
            anchors.right: viewport.right
            anchors.bottom: viewport.bottom
            anchors.bottomMargin: root.bottomChrome

            onPressedChanged:
            {
                if (pressed)
                {
                    root.scrollRestorePending = false
                    root.leaveReadAloud()
                }
            }
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

        TapHandler
        {
            acceptedDevices: PointerDevice.TouchScreen

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
                    pinch.gesturePagePoint = root.pagePointUnder(root.viewportPointOf(pinch.centroid.scenePosition))

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

    DesktopScrollBar
    {
        id: documentBar

        anchors.top: viewport.top
        anchors.topMargin: root.topChrome
        anchors.right: viewport.right
        anchors.bottom: viewport.bottom
        anchors.bottomMargin: root.bottomChrome
        orientation: Qt.Vertical
        size: root.documentSize

        onPositionChanged:
        {
            if (documentBar.pressed)
            {
                root.scrollToDocumentPosition(documentBar.position)
            }
        }
    }

    Binding
    {
        target: documentBar
        property: "position"
        value: root.documentPosition
        when: !documentBar.pressed
    }

    DesktopReaderAutoscroll
    {
        id: autoscroll

        anchors.fill: parent

        onStarted: root.leaveReadAloud()
        onScrollRequested: function(pixels) { root.autoscrollBy(pixels) }
    }
}
