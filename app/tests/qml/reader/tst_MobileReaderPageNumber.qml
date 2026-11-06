import QtQuick
import QtTest

TestCase
{
    id: test
    name: "MobileReaderPageNumber"
    width: 400
    height: 200
    visible: true
    when: windowShown

    function init()
    {
        failOnWarning(/.*/)
    }

    SignalSpy
    {
        id: navigation
        signalName: "pageRequested"
    }

    function createPageNumber()
    {
        const component = Qt.createComponent("MobileReaderPageNumber.qml")
        compare(component.status, Component.Ready, component.errorString())
        const control = createTemporaryObject(component, test,
            { width: 180, height: 43, page: 11, pageCount: 633 })
        verify(control !== null)
        navigation.target = control
        navigation.clear()
        return control
    }

    function test_tappingThePageNumberAllowsAnExactPageJump()
    {
        const control = createPageNumber()
        mouseClick(control, 90, 20)
        compare(control.editing, true)
        const field = findChild(control, "reader_page_number_input")
        verify(field !== null)
        compare(field.text, "12")
        keyClick(Qt.Key_4)
        keyClick(Qt.Key_2)
        keyClick(Qt.Key_Return)
        compare(navigation.count, 1)
        compare(navigation.signalArguments[0][0], 41)
        compare(control.editing, false)
    }

    function test_invalidPagesAndCancelKeepTheCurrentPosition()
    {
        const control = createPageNumber()
        mouseClick(control, 90, 20)
        const field = findChild(control, "reader_page_number_input")
        field.text = "0"
        keyClick(Qt.Key_Return)
        compare(navigation.count, 0)
        compare(control.editing, true)
        field.text = "634"
        keyClick(Qt.Key_Return)
        compare(navigation.count, 0)
        keyClick(Qt.Key_Escape)
        compare(control.editing, false)
        compare(control.page, 11)
    }
}
