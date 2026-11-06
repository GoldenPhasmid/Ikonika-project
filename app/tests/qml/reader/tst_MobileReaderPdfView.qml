import QtQuick
import QtTest

TestCase
{
    id: test
    name: "MobileReaderPdfView"
    width: 400
    height: 700
    visible: true
    when: windowShown

    function init()
    {
        failOnWarning(/.*/)
    }

    Component
    {
        id: readerComponent

        MobileReaderPdfView
        {
            width: 400
            height: 700
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

    function test_scrollCrossesPagesWithoutMovingTheVisiblePage()
    {
        const view = createTemporaryObject(readerComponent, test)
        verify(view !== null)
        tryCompare(view, "ready", true)
        wait(0)
        const viewport = viewportOf(view)
        view.scrollRestorePending = false
        viewport.contentY = 500
        tryCompare(view, "pageIndex", 1)
        verify(viewport.contentHeight > viewport.height * 2)
        const next = view.currentSheet
        const before = next.y - viewport.contentY
        viewport.contentY += 450
        tryCompare(view, "pageIndex", 2)
        fuzzyCompare(next.y - viewport.contentY, before - 450, 0.1)
        viewport.contentY -= 450
        tryCompare(view, "pageIndex", 1)
    }

    function test_originalPageMarginsArePreservedByDefaultAndCanBeDisabled()
    {
        const component = Qt.createComponent("MobileReaderPdfView.qml")
        compare(component.status, Component.Ready, component.errorString())
        const view = createTemporaryObject(component, test,
            { width: 400, height: 700, source: "fixture.pdf" })
        tryCompare(view, "ready", true)
        tryVerify(function() { return Math.abs(view.renderScale - 2 / 3) < 0.001 })
        view.cropMargins = true
        tryVerify(function() { return view.renderScale > 0.8 })
        view.cropMargins = false
        tryVerify(function() { return Math.abs(view.renderScale - 2 / 3) < 0.001 })
    }

    function test_pageJumpKeepsOnlyThreeActivePages()
    {
        const view = createTemporaryObject(readerComponent, test)
        tryCompare(view, "ready", true)
        wait(0)
        view.goToPage(7)
        compare(view.pageIndex, 7)
        const viewport = viewportOf(view)
        let pages = []
        for (const child of viewport.contentItem.children)
        {
            if (child.page !== undefined && child.visible)
            {
                pages.push(child.page)
                compare(child.prefetch, false)
            }
        }
        compare(pages.sort((first, second) => first - second).join(","), "6,7,8")
    }

    function test_differentPageWidthsFitBeforeBecomingCurrent()
    {
        const view = createTemporaryObject(readerComponent, test)
        tryCompare(view, "ready", true)
        wait(0)
        const next = view.nextSheet
        fuzzyCompare(next.width, view.width, 0.1)
        fuzzyCompare(next.x, 0, 0.1)
        const scale = next.renderScale
        view.goToPage(1)
        wait(0)
        fuzzyCompare(next.width, view.width, 0.1)
        fuzzyCompare(next.renderScale, scale, 0.001)
        fuzzyCompare(view.previousSheet.width, view.width, 0.1)
    }

    function test_openingWaitsForTheRequestedPageToRender()
    {
        const view = createTemporaryObject(readerComponent, test, { initialPage: 5 })
        tryCompare(view, "ready", true)
        wait(0)
        compare(view.presented, false)
        view.nextSheet.rendered(false)
        wait(0)
        compare(view.presented, false)
        view.currentSheet.rendered(false)
        tryCompare(view, "presented", true)
        view.goToPage(8)
        compare(view.presented, true)
        view.source = "another.pdf"
        compare(view.presented, false)
    }

    function test_restoreUsesAnOffsetWithinTheSavedPage()
    {
        const view = createTemporaryObject(readerComponent, test,
            { initialPage: 5, initialZoom: 1, initialScroll: Qt.point(40, 100) })
        tryCompare(view, "ready", true)
        wait(0)
        compare(view.pageIndex, 5)
        const viewport = viewportOf(view)
        tryVerify(function()
        {
            return Math.abs(viewport.contentY - view.currentSheet.y + view.pageMargin - 100) < 0.1
        })
        fuzzyCompare(viewport.contentX, 40, 0.1)
    }

    function test_dragContinuesIntoTheNextPage()
    {
        const view = createTemporaryObject(readerComponent, test)
        tryCompare(view, "ready", true)
        wait(0)
        mouseDrag(view, 200, 600, 0, -450, Qt.LeftButton)
        tryVerify(function() { return view.pageIndex > 0 })
    }

    function test_restoreAtFitWidthKeepsTheSavedPage()
    {
        const view = createTemporaryObject(readerComponent, test,
            { initialPage: 5, initialScroll: Qt.point(0, -100) })
        tryCompare(view, "ready", true)
        wait(0)
        compare(view.pageIndex, 5)
        const viewport = viewportOf(view)
        tryVerify(function()
        {
            return Math.abs((viewport.contentY - view.currentSheet.y + view.pageMargin)
                            / view.renderScale + 100) < 0.1
        })
    }

    function test_scrollingAcrossDifferentPageHeightsKeepsTheSameVisiblePoint()
    {
        const view = createTemporaryObject(readerComponent, test)
        tryCompare(view, "ready", true)
        wait(0)
        view.goToPage(3)
        const viewport = viewportOf(view)
        const next = view.nextSheet
        const before = next.y - viewport.contentY
        viewport.contentY += 450
        tryCompare(view, "pageIndex", 4)
        fuzzyCompare(next.y - viewport.contentY, before - 450, 0.1)
        const following = view.nextSheet
        const secondBefore = following.y - viewport.contentY
        viewport.contentY += 700
        tryCompare(view, "pageIndex", 5)
        fuzzyCompare(following.y - viewport.contentY, secondBefore - 700, 0.1)
    }

    function test_fitTracksWidthAndZoomStaysAnchored()
    {
        const view = createTemporaryObject(readerComponent, test)
        tryCompare(view, "ready", true)
        wait(0)
        view.width = 600
        tryCompare(view, "renderScale", 1)
        view.goToPage(3)
        const viewport = viewportOf(view)
        const spot = Qt.point(200, 300)
        const sceneSpot = view.mapToItem(null, spot)
        const before = view.pagePointUnder(spot)
        view.toggleZoom(sceneSpot)
        fuzzyCompare(view.renderScale, 2.2, 0.01)
        fuzzyCompare(view.pagePointUnder(spot).y, before.y, 0.1)
        fuzzyCompare(view.pagePointUnder(spot).x, before.x, 0.1)
        view.goToPage(100)
        compare(view.pageIndex, 11)
        compare(view.nextSheet, null)
        view.goToPage(-100)
        compare(view.pageIndex, 0)
        compare(view.previousSheet, null)
    }
}
