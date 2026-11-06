import QtQuick
import QtTest

TestCase
{
    id: test
    name: "DesktopReaderTextView"
    width: 800
    height: 600
    visible: true
    when: windowShown

    Component
    {
        id: fixture

        DesktopReaderTextView
        {
            width: 800
            height: 600

            Item
            {
                parent: contentItem

                TextEdit
                {
                    id: selection
                    x: 50
                    y: 60
                    width: 650
                    text: "A short selection followed by the rest of the paragraph.\nA second selected line with more text."
                    font.pixelSize: 20
                    readOnly: true
                    selectByMouse: true
                }
            }

            function selectThrough(end)
            {
                selection.select(0, end)
                claimSelection(selection)
                return selection
            }
        }
    }

    function test_selectionEndpoint_data()
    {
        return [{ tag: "single line", end: 17 }, { tag: "multiple lines", end: 70 }]
    }

    function test_selectionEndpoint(data)
    {
        failOnWarning(/.*/)
        const view = createTemporaryObject(fixture, test)
        verify(view !== null)
        const selection = view.selectThrough(data.end)
        const cursor = selection.positionToRectangle(data.end)
        const endpoint = selection.mapToItem(view, Qt.point(cursor.x, cursor.y + cursor.height / 2))
        const geometry = view.selectionGeometry()
        verify(geometry !== null)
        compare(geometry.endX, endpoint.x)
        compare(geometry.endY, endpoint.y)
        verify(geometry.endX < geometry.right - 100)
    }
}
