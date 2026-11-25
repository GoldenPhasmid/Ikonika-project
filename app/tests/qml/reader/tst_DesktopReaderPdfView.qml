import QtQuick
import QtTest

TestCase
{
    id: test
    name: "DesktopReaderPdfView"
    width: 800
    height: 600
    visible: true
    when: windowShown

    Component
    {
        id: readerComponent

        DesktopReaderPdfView
        {
            width: 800
            height: 600
            source: "fixture.pdf"
            cropMargins: false
        }
    }

    function viewportOf(item)
    {
        for (const child of item.children)
        {
            if (child.contentY !== undefined && child.contentHeight !== undefined)
            {
                return child
            }
        }

        fail("Reader has no scroll viewport")
    }

    function openReader()
    {
        const view = createTemporaryObject(readerComponent, test)
        verify(view !== null)
        tryCompare(view, "ready", true)
        wait(0)
        view.scrollRestorePending = false

        return view
    }

    function test_clickOnPdfLinkJumpsToItsDestination()
    {
        const view = openReader()
        const viewport = viewportOf(view)
        view.topChrome = 32
        let lookups = 0
        view.wordPicked.connect(function() { ++lookups })

        mouseClick(view.currentSheet, 220 * view.renderScale, 180 * view.renderScale)
        compare(lookups, 1)

        mouseClick(view.currentSheet, 150 * view.renderScale, 180 * view.renderScale)
        compare(view.pageIndex, 4)
        compare(lookups, 1)
        fuzzyCompare(view.currentSheet.y + 225 * view.renderScale - viewport.contentY, view.topChrome, 0.1)
    }

    function test_middleClickStartsAutoscrollAndScrollsTowardThePointer()
    {
        const view = openReader()
        const viewport = viewportOf(view)
        const start = viewport.contentY

        mouseClick(view, 400, 200, Qt.MiddleButton)
        compare(view.autoscrolling, true)
        mouseMove(view, 400, 500)
        tryVerify(function() { return viewport.contentY > start + 100 })
        mouseMove(view, 400, 200)
        const turned = viewport.contentY
        mouseMove(view, 400, 20)
        tryVerify(function() { return viewport.contentY < turned - 20 })
    }

    function test_autoscrollContinuesIntoTheNextPage()
    {
        const view = openReader()

        mouseClick(view, 400, 100, Qt.MiddleButton)
        mouseMove(view, 400, 590)
        tryVerify(function() { return view.pageIndex > 0 }, 10000)
        compare(view.autoscrolling, true)
    }

    function test_secondMiddleClickStopsAutoscroll()
    {
        const view = openReader()
        const viewport = viewportOf(view)

        mouseClick(view, 400, 200, Qt.MiddleButton)
        mouseMove(view, 400, 500)
        tryVerify(function() { return viewport.contentY > 0 })
        mouseClick(view, 400, 500, Qt.MiddleButton)
        compare(view.autoscrolling, false)
        const stopped = viewport.contentY
        wait(100)
        compare(viewport.contentY, stopped)
    }

    function test_holdingMiddleButtonScrollsOnlyWhileHeld()
    {
        const view = openReader()
        const viewport = viewportOf(view)

        mousePress(view, 400, 200, Qt.MiddleButton)
        compare(view.autoscrolling, true)
        mouseMove(view, 400, 500, 0, Qt.MiddleButton)
        tryVerify(function() { return viewport.contentY > 0 })
        mouseRelease(view, 400, 500, Qt.MiddleButton)
        compare(view.autoscrolling, false)
    }

    function test_releaseInsideTheDeadZoneKeepsAutoscrollRunning()
    {
        const view = openReader()

        mousePress(view, 400, 200, Qt.MiddleButton)
        mouseRelease(view, 400, 200, Qt.MiddleButton)
        compare(view.autoscrolling, true)
        mouseClick(view, 400, 200, Qt.LeftButton)
        compare(view.autoscrolling, false)
    }

    function test_wheelStopsAutoscroll()
    {
        const view = openReader()

        mouseClick(view, 400, 200, Qt.MiddleButton)
        compare(view.autoscrolling, true)
        mouseWheel(view, 400, 300, 0, -120)
        compare(view.autoscrolling, false)
    }

    function test_leftClickOnThePageDoesNotStartAutoscroll()
    {
        const view = openReader()

        mouseClick(view, 400, 200, Qt.LeftButton)
        compare(view.autoscrolling, false)
    }

    function documentBarOf(view)
    {
        for (const child of view.children)
        {
            if (child.orientation === Qt.Vertical && child.size !== undefined)
            {
                return child
            }
        }

        fail("Reader has no document scroll bar")
    }

    function test_documentBarCoversTheWholeDocument()
    {
        const view = openReader()
        const bar = documentBarOf(view)

        verify(bar.size > 0 && bar.size < 0.2)
        compare(bar.visible, true)
        fuzzyCompare(bar.position, 0, 0.001)
    }

    function test_documentBarFollowsThePageBeingRead()
    {
        const view = openReader()
        const bar = documentBarOf(view)

        view.goToPage(6)
        tryVerify(function() { return Math.abs(bar.position - 6 / 12) < 0.01 })
        view.goToPage(9)
        tryVerify(function() { return Math.abs(bar.position - 9 / 12) < 0.01 })
        view.goToPage(0)
        tryVerify(function() { return bar.position < 0.01 })
    }

    function test_documentBarMovesWithinAPage()
    {
        const view = openReader()
        const bar = documentBarOf(view)
        const viewport = viewportOf(view)

        view.goToPage(3)
        tryVerify(function() { return Math.abs(bar.position - 3 / 12) < 0.01 })
        const before = bar.position
        viewport.contentY += 200
        tryVerify(function() { return bar.position > before })
        verify(bar.position < 4 / 12)
    }

    function test_draggingTheDocumentBarJumpsToThatPartOfTheDocument()
    {
        const view = openReader()
        const bar = documentBarOf(view)

        mouseDrag(bar, bar.width / 2, 10, 0, 250, Qt.LeftButton)
        tryVerify(function() { return view.pageIndex >= 3 && view.pageIndex <= 6 })
        tryVerify(function() { return Math.abs(bar.position - view.pageIndex / 12) < 0.12 })
    }

    function test_draggingTheDocumentBarToTheEndReachesTheLastPages()
    {
        const view = openReader()
        const bar = documentBarOf(view)

        mouseDrag(bar, bar.width / 2, 10, 0, 800, Qt.LeftButton)
        tryVerify(function() { return view.pageIndex >= 10 })
    }
}
