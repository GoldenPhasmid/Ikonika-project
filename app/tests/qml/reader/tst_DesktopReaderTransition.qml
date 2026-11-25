import QtQuick
import QtTest

TestCase
{
    id: test
    name: "DesktopReaderTransition"
    width: 400
    height: 300
    visible: true
    when: windowShown

    Component
    {
        id: transitionComponent

        DesktopReaderTransition
        {
            width: 400
            height: 300
        }
    }

    function test_openWaitsForContentAndBlocksInput()
    {
        const transition = createTemporaryObject(transitionComponent, test, { contentReady: false })
        verify(transition.open())
        compare(transition.opening, true)
        compare(transition.running, true)
        tryCompare(transition, "awaitingContent", true)
        compare(transition.shelfShown, 0)
        compare(transition.open(), false)
    }

    function test_finishOpenWhileWaitingRestoresTheShelf()
    {
        const transition = createTemporaryObject(transitionComponent, test, { contentReady: false })
        transition.open()
        tryCompare(transition, "awaitingContent", true)

        transition.finishOpen()

        compare(transition.opening, false)
        compare(transition.running, false)
        compare(transition.awaitingContent, false)
        compare(transition.shelfShown, 1)
        compare(transition.shelfScale, 1)
        compare(transition.pageShown, 1)
        compare(transition.chromeShown, 1)
    }

    function test_closeIsAvailableAgainAfterAnInterruptedOpen()
    {
        const transition = createTemporaryObject(transitionComponent, test, { contentReady: false })
        let shut = 0
        transition.open()
        tryCompare(transition, "awaitingContent", true)
        compare(transition.close(function() { ++shut }), false)

        transition.finishOpen()

        compare(transition.close(function() { ++shut }), true)
        tryCompare(transition, "running", false)
        compare(shut, 1)
        compare(transition.shelfShown, 1)
    }

    function test_openFinishesOnceTheContentArrives()
    {
        const transition = createTemporaryObject(transitionComponent, test, { contentReady: false })
        transition.open()
        tryCompare(transition, "awaitingContent", true)

        transition.contentReady = true

        tryCompare(transition, "running", false)
        compare(transition.opening, false)
        compare(transition.pageShown, 1)
        compare(transition.shelfShown, 1)
    }
}
