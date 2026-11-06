import QtQuick
import QtTest

TestCase
{
    id: test
    name: "MobileReaderFrame"
    width: 400
    height: 700
    visible: true
    when: windowShown

    function test_controlsOverlayThePageWithoutChangingReadingPosition()
    {
        failOnWarning(/.*/)
        const component = Qt.createComponent("ReaderFrameFixture.qml")
        compare(component.status, Component.Ready, component.errorString())
        const frame = createTemporaryObject(component, test)
        const pdf = findChild(frame, "pdf")
        const navigation = findChild(frame, "navigation")
        verify(pdf !== null)
        verify(navigation !== null)
        verify(navigation.mapToItem(frame, Qt.point(0, 0)).y >= 600)
        tryCompare(pdf, "ready", true)
        wait(0)
        pdf.goToPage(3)
        const position = pdf.pagePointUnder(Qt.point(200, 300))
        const origin = pdf.mapToItem(test, Qt.point(0, 0))
        compare(pdf.height, 700)
        compare(pdf.width, 400)
        frame.controlsVisible = false
        wait(0)
        compare(pdf.height, 700)
        compare(pdf.mapToItem(test, Qt.point(0, 0)), origin)
        compare(pdf.pagePointUnder(Qt.point(200, 300)), position)
        frame.controlsVisible = true
        wait(0)
        compare(pdf.height, 700)
        compare(pdf.mapToItem(test, Qt.point(0, 0)), origin)
        compare(pdf.pagePointUnder(Qt.point(200, 300)), position)
        compare(pdf.pageIndex, 3)
    }
}
