import QtQuick

ReaderPdfPage
{
    id: root

    required property int slot
    required property Item view
    required property Flickable viewport

    readonly property int slotPage: root.view.current - 1 + (((root.slot - (root.view.current - 1)) % 3) + 3) % 3
    readonly property bool inUse: root.ready && root.slotPage >= 0 && root.slotPage < root.pageCount

    property var selectionRectangles: []
    property string selectionText: ""
    property string selectionSentence: ""
    property real selectionScale: 1

    readonly property real selectionStretch: root.renderScale / root.selectionScale

    function clearSelection()
    {
        root.selectionRectangles = []
        root.selectionText = ""
    }

    function showSelection(found)
    {
        root.selectionScale = root.renderScale
        root.selectionRectangles = found.rectangles
        root.selectionText = found.text
        root.selectionSentence = found.sentence || ""
    }

    document: root.view.document
    paperColour: root.view.paperColour
    inkColour: root.view.inkColour
    renderScale: root.view.renderScale
    prefetch: false
    visible: root.inUse

    x: root.view.sheetX
    y: root === root.view.previousSheet ? root.view.stackTop
        : root === root.view.currentSheet ? root.view.currentTop
        : root.view.currentTop + root.view.currentSheet.height + root.view.gap

    width: root.pageSize.width * root.renderScale
    height: root.pageSize.height * root.renderScale

    visibleArea: root.inUse
        ? Qt.rect(root.viewport.contentX - root.x, root.viewport.contentY - root.y, root.viewport.width, root.viewport.height)
        : Qt.rect(0, 0, 0, 0)

    onPageChanged: root.clearSelection()
    onRendered: if (root === root.view.currentSheet) root.view.pageDrawn = true

    Binding on page
    {
        when: root.inUse
        value: root.slotPage
        restoreMode: Binding.RestoreNone
    }

    Rectangle
    {
        anchors.fill: parent
        anchors.margins: -AppMetrics.borderWidth
        z: -1

        color: root.paperColour
        border.width: AppMetrics.borderWidth
        border.color: AppTheme.colourBorder
    }

    DragHandler
    {
        id: selectionDrag

        acceptedDevices: PointerDevice.Mouse | PointerDevice.Stylus
        target: null
        cursorShape: Qt.IBeamCursor

        onActiveChanged:
        {
            if (selectionDrag.active)
            {
                root.view.clearSelection()

                return
            }

            const picked = root.selectionText.trim()

            if (picked.length > 0)
            {
                root.view.wordPicked(picked, root.selectionSentence)
            }
        }

        onCentroidChanged:
        {
            if (selectionDrag.active)
            {
                root.showSelection(root.selectionBetween(centroid.pressPosition, centroid.position))
            }
        }
    }

    TapHandler
    {
        acceptedDevices: PointerDevice.Mouse | PointerDevice.Stylus

        onTapped: function(eventPoint)
        {
            const found = root.selectWordAt(eventPoint.position)

            root.view.clearSelection()
            root.showSelection(found)

            if (found.text.length > 0)
            {
                root.view.wordPicked(found.text, root.selectionSentence)
            }
        }
    }

    HoverHandler
    {
        cursorShape: Qt.IBeamCursor
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

            color: AppTheme.faded(AppTheme.colourAccent, AppTheme.alphaHalf)
        }
    }
}
