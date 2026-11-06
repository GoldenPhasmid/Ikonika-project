import QtQuick
import QtQuick.Controls.Basic

ListView
{
    id: root

    property int textSize: AppMetrics.fontLead
    property int lineSpacing: 100
    property int textMargin: 0
    property int pageWidth: 100
    property color inkColour: AppTheme.colourText
    property string bookPath: ""
    property string fontFamily: AppMetrics.fontFamily
    property ReaderVocabulary vocabulary: null

    signal wordPicked(string word, string sentence)
    signal zoomRequested(bool closer)

    DictionaryController { id: contextLookup }

    readonly property int topParagraph:
    {
        const top = root.contentY + 1
        const index = root.indexAt(root.width / 2, top)

        return Math.max(0, index >= 0 ? index : root.indexAt(root.width / 2, top + root.spacing + 1))
    }
    readonly property real columnWidth: Math.round(root.width * root.pageWidth / 100)
    readonly property real textWidth: root.columnWidth - root.textMargin * 2

    property var selectedText: null

    readonly property bool styled: root.model !== null && root.model !== undefined && root.model.styled === true

    property real glideTarget: 0
    property bool autoscrolling: false
    property point autoscrollAnchor: Qt.point(0, 0)
    property real autoscrollPointerY: 0

    readonly property real scrollStart: root.originY - root.topMargin
    readonly property real scrollLimit: Math.max(root.scrollStart, root.originY + root.contentHeight + root.bottomMargin - root.height)

    function goTo(index)
    {
        wheelGlide.stop()
        root.positionViewAtIndex(Math.max(0, Math.min(root.count - 1, index)), ListView.Beginning)

        if (index <= 0)
        {
            root.contentY = root.scrollStart
        }
    }

    function scrollBy(pixels)
    {
        wheelGlide.stop()
        root.contentY = Math.max(root.scrollStart, Math.min(root.scrollLimit, root.contentY + pixels))
    }

    function glideBy(pixels)
    {
        const from = wheelGlide.running ? root.glideTarget : root.contentY

        root.glideTarget = Math.max(root.scrollStart, Math.min(root.scrollLimit, from + pixels))

        wheelGlide.stop()
        wheelGlide.from = root.contentY
        wheelGlide.to = root.glideTarget
        wheelGlide.start()
    }

    function startAutoscroll(position)
    {
        wheelGlide.stop()
        root.autoscrollAnchor = position
        root.autoscrollPointerY = position.y
        root.autoscrolling = true
    }

    function stopAutoscroll()
    {
        root.autoscrolling = false
    }

    function turnPage(forward)
    {
        root.scrollBy(root.height * 0.9 * (forward ? 1 : -1))
    }

    function isWordCharacter(character)
    {
        return character.toLowerCase() !== character.toUpperCase() || /[0-9'’\-]/.test(character)
    }

    function wordRange(text, index)
    {
        if (index < 0 || index >= text.length || !root.isWordCharacter(text.charAt(index)))
        {
            return null
        }

        let start = index
        let end = index + 1

        while (start > 0 && root.isWordCharacter(text.charAt(start - 1)))
        {
            --start
        }

        while (end < text.length && root.isWordCharacter(text.charAt(end)))
        {
            ++end
        }

        return { start: start, end: end }
    }

    function escapeHtml(text)
    {
        return text.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
    }

    function headingScale(level)
    {
        if (level === 1)
        {
            return 1.45
        }

        return level === 2 ? 1.3 : 1.15
    }

    function capitalization(transform)
    {
        if (transform === "uppercase")
        {
            return Font.AllUppercase
        }

        if (transform === "lowercase")
        {
            return Font.AllLowercase
        }

        return transform === "capitalize" ? Font.Capitalize : Font.MixedCase
    }

    function selectionText()
    {
        return root.selectedText === null ? "" : root.selectedText.selectedText
    }

    function clearSelection()
    {
        if (root.selectedText === null)
        {
            return
        }

        root.selectedText.deselect()
        root.selectedText = null
    }

    function isOverText(position)
    {
        const item = root.itemAt(position.x + root.contentX, position.y + root.contentY)

        if (item === null || item.pictured || item.rule)
        {
            return false
        }

        const local = item.mapFromItem(root, position.x, position.y)

        return local.y >= item.topGap && local.x >= item.textLeft && local.x <= item.textLeft + root.textWidth
    }

    function claimSelection(owner)
    {
        if (root.selectedText !== null && root.selectedText !== owner)
        {
            root.selectedText.deselect()
        }

        root.selectedText = owner
    }

    function selectionGeometry()
    {
        const owner = root.selectedText

        if (owner === null || owner.selectedText.length === 0)
        {
            return null
        }

        const first = owner.positionToRectangle(owner.selectionStart)
        const last = owner.positionToRectangle(owner.selectionEnd)
        const left = owner.parent.x + owner.x - root.contentX
        const top = owner.parent.y + owner.y - root.contentY
        const singleLine = first.y === last.y
        const selectionLeft = left + (singleLine ? Math.min(first.x, last.x) : 0)
        const selectionRight = left + (singleLine ? Math.max(first.x, last.x) : owner.width)

        return { top: top + first.y, bottom: top + last.y + last.height, endX: left + last.x, endY: top + last.y + last.height / 2,
                 left: selectionLeft, right: selectionRight, width: selectionRight - selectionLeft }
    }

    clip: true
    spacing: root.styled ? 0 : Math.round(root.textSize * 0.6)
    boundsBehavior: Flickable.StopAtBounds
    acceptedButtons: Qt.NoButton
    topMargin: AppMetrics.contentPadding
    bottomMargin: Math.round(root.height * AppMetrics.readerEndSpaceShare)

    Accessible.role: Accessible.Document

    ScrollBar.vertical: DesktopScrollBar {}

    NumberAnimation
    {
        id: wheelGlide

        target: root
        property: "contentY"
        duration: AppMetrics.motionBase
        easing.type: Easing.OutCubic
    }

    WheelHandler
    {
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        acceptedModifiers: Qt.ControlModifier
        target: null

        onWheel: function(event)
        {
            const delta = event.angleDelta.y !== 0 ? event.angleDelta.y : event.pixelDelta.y

            if (delta !== 0)
            {
                root.stopAutoscroll()
                root.zoomRequested(delta > 0)
            }

            event.accepted = true
        }
    }

    WheelHandler
    {
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        acceptedModifiers: Qt.NoModifier

        onWheel: function(event)
        {
            if (event.pixelDelta.y !== 0)
            {
                root.scrollBy(-event.pixelDelta.y)

                return
            }

            root.glideBy(-event.angleDelta.y / 120 * AppMetrics.readerWheelStep)
        }
    }

    PointHandler
    {
        id: pagePointer

        enabled: !root.autoscrolling
        acceptedDevices: PointerDevice.Mouse | PointerDevice.Stylus
        acceptedButtons: Qt.LeftButton

        onActiveChanged:
        {
            if (pagePointer.active && !root.isOverText(root.mapFromItem(null, pagePointer.point.scenePosition)))
            {
                root.clearSelection()
            }
        }
    }

    FrameAnimation
    {
        running: root.autoscrolling

        onTriggered:
        {
            const distance = root.autoscrollPointerY - root.autoscrollAnchor.y
            const beyond = Math.max(0, Math.abs(distance) - AppMetrics.readerAutoscrollDeadZone)

            if (beyond > 0)
            {
                root.scrollBy(Math.sign(distance) * beyond * AppMetrics.readerAutoscrollSpeed * frameTime)
            }
        }
    }

    MouseArea
    {
        parent: root
        anchors.fill: parent
        z: 3
        acceptedButtons: Qt.MiddleButton

        onPressed: function(mouse)
        {
            if (root.autoscrolling)
            {
                root.stopAutoscroll()

                return
            }

            root.startAutoscroll(Qt.point(mouse.x, mouse.y))
        }

        onPositionChanged: function(mouse) { root.autoscrollPointerY = mouse.y }

        onReleased:
        {
            if (Math.abs(root.autoscrollPointerY - root.autoscrollAnchor.y) > AppMetrics.readerAutoscrollDeadZone)
            {
                root.stopAutoscroll()
            }
        }
    }

    MouseArea
    {
        parent: root
        anchors.fill: parent
        z: 2
        visible: root.autoscrolling
        hoverEnabled: true
        acceptedButtons: Qt.AllButtons
        cursorShape: Qt.SizeVerCursor

        onPositionChanged: function(mouse) { root.autoscrollPointerY = mouse.y }
        onReleased: root.stopAutoscroll()
        onWheel: function(wheel) { root.stopAutoscroll() }
    }

    Rectangle
    {
        parent: root
        z: 4
        visible: root.autoscrolling
        x: root.autoscrollAnchor.x - width / 2
        y: root.autoscrollAnchor.y - height / 2
        width: AppMetrics.readerAutoscrollAnchorSize
        height: width
        radius: width / 2
        color: AppTheme.colourCard
        border.width: AppMetrics.borderWidth
        border.color: AppTheme.colourBorder

        Rectangle
        {
            anchors.centerIn: parent
            width: parent.width / 5
            height: width
            radius: width / 2
            color: AppTheme.colourText
        }
    }

    delegate: Item
    {
        id: paragraph

        required property string display
        required property int heading
        required property int index
        required property string markup
        required property real marginTop
        required property real marginTopWidth
        required property real previousMarginBottom
        required property real previousMarginBottomWidth
        required property real textIndent
        required property real textIndentWidth
        required property real marginLeft
        required property real marginLeftWidth
        required property real marginRight
        required property real marginRightWidth
        required property string image
        required property int imageWidth
        required property int imageHeight
        required property bool rule

        readonly property bool pictured: paragraph.image.length > 0
        readonly property real pictureScale: paragraph.imageWidth > 0 && paragraph.imageHeight > 0
                                             ? Math.min(1, root.textWidth / paragraph.imageWidth,
                                                        root.height * AppMetrics.readerImageHeightShare / paragraph.imageHeight)
                                             : 1
        readonly property real textLeft: (paragraph.width - root.columnWidth) / 2 + root.textMargin
        required property string textAlign
        required property real fontScale
        required property bool bold
        required property bool italic
        required property bool smallCaps
        required property string textTransform

        readonly property real topGap:
        {
            if (paragraph.index === 0)
            {
                return 0
            }

            if (root.styled)
            {
                return Math.max(paragraph.previousMarginBottom * root.textSize + paragraph.previousMarginBottomWidth * root.textWidth,
                                paragraph.marginTop * root.textSize + paragraph.marginTopWidth * root.textWidth)
            }

            return paragraph.heading > 0 ? root.textSize : 0
        }

        width: ListView.view.width
        height: (paragraph.pictured ? picture.height : (paragraph.rule ? root.textSize : paragraphText.implicitHeight))
                + paragraph.topGap

        Rectangle
        {
            visible: paragraph.rule
            x: paragraph.textLeft + (root.textWidth - width) / 2
            y: paragraph.topGap + (root.textSize - height) / 2
            width: Math.round(root.textWidth / 3)
            height: AppMetrics.borderWidth
            color: root.inkColour
            opacity: 0.4
        }

        Image
        {
            id: picture

            visible: paragraph.pictured
            x: paragraph.textLeft + (root.textWidth - width) / 2
            y: paragraph.topGap
            width: paragraph.imageWidth > 0 ? Math.round(paragraph.imageWidth * paragraph.pictureScale)
                                            : Math.min(root.textWidth, implicitWidth)
            height: paragraph.imageHeight > 0 ? Math.round(paragraph.imageHeight * paragraph.pictureScale)
                                              : (implicitWidth > 0 ? width * implicitHeight / implicitWidth : 0)
            source: paragraph.pictured ? "image://book/" + encodeURIComponent(root.bookPath) + "/" + encodeURIComponent(paragraph.image) : ""
            sourceSize: Qt.size(Math.round(width * Screen.devicePixelRatio), Math.round(height * Screen.devicePixelRatio))
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            smooth: true

            Accessible.role: Accessible.Graphic
            Accessible.name: qsTr("Illustration")
        }

        TextEdit
        {
            id: paragraphText

            visible: !paragraph.pictured && !paragraph.rule

            x: paragraph.textLeft
            y: paragraph.topGap
            width: root.textWidth

            text: "<p" + (paragraph.textAlign.length > 0 ? " align='" + paragraph.textAlign + "'" : "")
                + " style='margin:0; line-height:" + root.lineSpacing + "%"
                + "; margin-left:" + Math.round(paragraph.marginLeft * root.textSize + paragraph.marginLeftWidth * root.textWidth) + "px"
                + "; margin-right:" + Math.round(paragraph.marginRight * root.textSize + paragraph.marginRightWidth * root.textWidth) + "px"
                + (paragraph.textIndent !== 0 || paragraph.textIndentWidth !== 0
                   ? "; text-indent:" + Math.round(paragraph.textIndent * root.textSize + paragraph.textIndentWidth * root.textWidth) + "px"
                   : "")
                + "'>"
                + (paragraph.markup.length > 0 ? paragraph.markup : root.escapeHtml(paragraph.display)) + "</p>"
            textFormat: TextEdit.RichText
            color: root.inkColour
            font.pixelSize: Math.round(root.textSize * (root.styled ? paragraph.fontScale
                                                                    : (paragraph.heading > 0 ? root.headingScale(paragraph.heading) : 1)))
            font.weight: root.styled ? (paragraph.bold ? Font.Bold : Font.Normal)
                                     : (paragraph.heading > 0 ? Font.DemiBold : Font.Normal)
            font.italic: paragraph.italic
            font.capitalization: paragraph.smallCaps ? Font.SmallCaps : root.capitalization(paragraph.textTransform)
            font.family: root.fontFamily

            wrapMode: TextEdit.Wrap
            readOnly: true
            activeFocusOnPress: false
            selectByMouse: true
            persistentSelection: true
            cursorVisible: false
            selectionColor: AppTheme.colourAccent
            selectedTextColor: AppTheme.colourAccentText

            Accessible.role: paragraph.heading > 0 ? Accessible.Heading : Accessible.Paragraph
            Accessible.name: paragraph.display

            PointHandler
            {
                id: pointer

                enabled: !root.autoscrolling
                acceptedDevices: PointerDevice.Mouse | PointerDevice.Stylus
                acceptedButtons: Qt.LeftButton

                property point releasedAt

                onPointChanged: if (pointer.active) pointer.releasedAt = pointer.point.position

                onActiveChanged:
                {
                    if (pointer.active)
                    {
                        pointer.releasedAt = pointer.point.position
                        root.claimSelection(paragraphText)

                        return
                    }

                    Qt.callLater(pointer.pick)
                }

                function pick()
                {
                    if (paragraph.display.substring(paragraphText.selectionStart, paragraphText.selectionEnd).trim().length > 0)
                    {
                        const first = root.wordRange(paragraph.display, paragraphText.selectionStart)
                        const last = root.wordRange(paragraph.display, paragraphText.selectionEnd - 1)
                        let start = first !== null ? first.start : paragraphText.selectionStart
                        let end = last !== null ? last.end : paragraphText.selectionEnd

                        while (start < end && /\s/.test(paragraph.display.charAt(start)))
                        {
                            ++start
                        }

                        while (end > start && /\s/.test(paragraph.display.charAt(end - 1)))
                        {
                            --end
                        }

                        paragraphText.select(start, end)
                        root.wordPicked(paragraph.display.substring(start, end).trim(), contextLookup.sentenceAt(paragraph.display, start))

                        return
                    }

                    const range = root.wordRange(paragraph.display,
                                                 hitTest.characterAt(pointer.releasedAt.x, pointer.releasedAt.y))

                    if (range === null)
                    {
                        return
                    }

                    paragraphText.select(range.start, range.end)
                    root.wordPicked(paragraph.display.substring(range.start, range.end), contextLookup.sentenceAt(paragraph.display, range.start))
                }
            }

            ReaderTextHitTest
            {
                id: hitTest

                textDocument: paragraphText.textDocument
            }

            ReaderVocabularyHighlighter
            {
                textDocument: root.vocabulary !== null && root.vocabulary.enabled ? paragraphText.textDocument : null
                vocabulary: root.vocabulary
                learningColour: AppTheme.faded(AppTheme.colourAccent, AppTheme.alphaMedium)
            }

            HoverHandler
            {
                cursorShape: Qt.IBeamCursor
            }
        }
    }
}
