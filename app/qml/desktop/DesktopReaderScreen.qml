import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../ReaderTones.js" as Tones
import "../ReaderShelf.js" as Shelf

Item
{
    id: root

    required property var store

    signal addWordRequested(string word, string translation, string partOfSpeech, string hint)
    signal lookUpRequested(string word)
    signal messageRequested(string message)
    signal crumbsChanging()

    readonly property bool reading: reader.reading
    readonly property string currentTitle: reader.currentTitle
    readonly property bool isPdf: reader.isPdf
    readonly property bool cannotRead: root.reading && (reader.unreadable || (reader.isPdf && pdfView.failed))
    readonly property bool readable: root.reading && !root.cannotRead
    readonly property int bookCount: reader.documents.length
    readonly property bool freeAvailable: reader.freeBooks.length > 0
    property bool freeShown: false
    property string levelFilter: ""
    readonly property int shelfCount: root.freeShown ? reader.freeBooks.length : root.bookCount
    readonly property real chromeShown: bookMorph.chromeShown
    readonly property ReaderReadAloud readAloud: reader.readAloud
    readonly property bool readAloudOffered: root.readable && !reader.isPdf
    property bool voiceNoticeShown: false

    property bool immersive: false
    readonly property string sortOrder: Shelf.sortOrders.indexOf(AppReaderSettings.librarySortOrder) >= 0
                                        ? AppReaderSettings.librarySortOrder : Shelf.sortOrders[0]
    readonly property bool sortReversed: AppReaderSettings.librarySortReversed

    readonly property var sortDirections: [
        [qsTr("Newest first"), qsTr("Oldest first")],
        [qsTr("Newest first"), qsTr("Oldest first")],
        [qsTr("A to Z"), qsTr("Z to A")],
        [qsTr("In progress first"), qsTr("Finished first")]
    ]

    readonly property string searchTerm: librarySearch.term
    readonly property var shelf: root.freeShown ? Shelf.arrange(reader.freeBooks, "", root.searchTerm, false, root.levelFilter)
                                                : Shelf.arrange(reader.documents, root.sortOrder, root.searchTerm, root.sortReversed)

    onFreeAvailableChanged:
    {
        if (!root.freeAvailable)
        {
            root.freeShown = false
        }
        else if (root.bookCount === 0)
        {
            root.freeShown = true
        }
    }

    readonly property int tileColumns: Math.max(1, Math.floor((library.columnWidth + AppMetrics.gapMedium)
                                                              / (AppMetrics.coverTileMinimumWidth + AppMetrics.gapMedium)))
    readonly property real tileWidth: Math.floor((library.columnWidth - AppMetrics.gapMedium * (root.tileColumns - 1))
                                                 / root.tileColumns)
    property bool contentsOpen: false

    property int pdfPendingPage: 0
    property real pdfPendingZoom: 0
    property point pdfPendingScroll: Qt.point(0, 0)

    readonly property int positionCount: reader.isPdf ? pdfView.pageCount : textView.count
    readonly property int positionIndex: reader.isPdf ? pdfView.pageIndex : textView.topParagraph

    onPositionCountChanged:
    {
        if (root.readable && root.positionCount > 0)
        {
            reader.savePositionCount(root.positionCount)
        }
    }

    readonly property var contentsEntries: reader.isPdf ? pdfView.outlineEntries : reader.chapters
    readonly property var pageTone: Tones.find(reader.pageTone)
    readonly property var textTone: Tones.findText(AppReaderSettings.textTone)
    readonly property color textPaper: root.textTone.paper.length > 0 ? root.textTone.paper : AppTheme.colourPanel
    readonly property color textInk: root.textTone.ink.length > 0 ? root.textTone.ink : AppTheme.colourText

    function refresh()
    {
        if (!reader.reading)
        {
            reader.refresh()

            if (root.bookCount === 0)
            {
                root.freeShown = root.freeAvailable
            }
        }
    }

    function openBook()
    {
        reader.pickAndOpen()
    }

    function openPath(path)
    {
        root.savePosition()
        reader.openPath(path)
    }

    function focusSearch()
    {
        librarySearch.forceActiveFocus()
        librarySearch.selectAll()
    }

    function confirmRemove(document)
    {
        const inCloud = document.cloudId.length > 0

        cloudDeleteDialog.ask(inCloud ? qsTr("Remove \"%1\" from this device?").arg(document.title)
                                      : qsTr("Remove \"%1\" from the library?").arg(document.title),
                              inCloud ? qsTr("The book stays in the cloud, and you can download it again.")
                                      : document.free ? qsTr("You can download it again from Free books.")
                                                      : qsTr("The file stays where it is on your computer."),
                              qsTr("Remove"),
                              function() { reader.removeDocument(document.id) })
    }

    function confirmCloudDelete(document)
    {
        cloudDeleteDialog.ask(qsTr("Delete \"%1\" from the cloud?").arg(document.title),
                              qsTr("The book is removed from the cloud and from every device that downloaded it."),
                              qsTr("Delete"),
                              function() { reader.deleteCloudBook(document.cloudId) })
    }

    function savePosition()
    {
        if (!reader.reading)
        {
            return
        }

        if (reader.isPdf)
        {
            reader.savePosition(root.pdfPendingPage)
            reader.saveZoom(root.pdfPendingZoom)
            reader.saveScroll(root.pdfPendingScroll)

            return
        }

        if (!reader.loading)
        {
            reader.savePosition(textView.topParagraph)
        }
    }

    function landingFor(isPdf)
    {
        const page = bookMorph.rectIn(pageArea)
        const height = page.height - AppMetrics.gapLarge * 2

        if (isPdf)
        {
            const width = Math.min(page.width - AppMetrics.gapLarge * 2, height * AppMetrics.readerPageAspect)

            return Qt.rect(page.x + (page.width - width) / 2, page.y + AppMetrics.gapLarge, width, height)
        }

        return Qt.rect(page.x + (page.width - textView.columnWidth) / 2, page.y + AppMetrics.gapLarge, textView.columnWidth, height)
    }

    function tileFor(id)
    {
        shelfFlow.forceLayout()

        for (let index = 0; index < shelfRepeater.count; ++index)
        {
            const tile = shelfRepeater.itemAt(index)

            if (tile && tile.modelData.id === id)
            {
                return tile
            }
        }

        return null
    }

    function openFromTile(tile, document)
    {
        bookMorph.open(tile, root.landingFor(document.format === "pdf"))
        reader.openDocument(document.id)

        if (bookMorph.morphing && !reader.reading)
        {
            bookMorph.finishOpen()
        }
    }

    function closeBook()
    {
        root.contentsOpen = false
        root.immersive = false

        const id = reader.currentId
        const tile = root.readable ? root.tileFor(id) : null
        const landing = root.landingFor(reader.isPdf)

        if (!bookMorph.close(tile, landing, bookMorph.rectIn(pageArea), pageArea.color, root.closeNow,
                             function() { return root.tileFor(id) }))
        {
            root.closeNow()
        }
    }

    function closeNow()
    {
        root.savePosition()

        root.contentsOpen = false
        root.immersive = false

        reader.closeDocument()
        reader.refresh()

        if (bookMorph.morphing)
        {
            root.crumbsChanging()
        }
    }

    function removeCurrentBook()
    {
        const id = reader.currentId

        root.closeNow()
        reader.removeDocument(id)
    }

    function goToPosition(index)
    {
        if (reader.isPdf)
        {
            pdfView.goToPage(index)

            return
        }

        textView.goTo(index)
    }

    function turnPage(forward)
    {
        if (reader.isPdf)
        {
            pdfView.goToPage(pdfView.pageIndex + (forward ? 1 : -1))

            return
        }

        textView.turnPage(forward)
    }

    function zoom(closer)
    {
        if (reader.isPdf)
        {
            pdfView.zoomBy(closer ? AppMetrics.readerZoomStep : 1 / AppMetrics.readerZoomStep)

            return
        }

        AppReaderSettings.textSize = AppReaderSettings.textSize + (closer ? 1 : -1)
    }

    function scrollBy(pixels)
    {
        if (reader.isPdf)
        {
            pdfView.scrollBy(pixels)

            return
        }

        textView.scrollBy(pixels)
    }

    function toggleContents()
    {
        root.contentsOpen = !root.contentsOpen

        if (root.contentsOpen && reader.isPdf && pdfView.outlineEntries.length === 0)
        {
            pdfView.requestOutline()
        }
    }

    function toggleReadAloud()
    {
        keys.forceActiveFocus()

        if (reader.readAloud.started)
        {
            reader.readAloud.stop()

            return
        }

        if (!reader.readAloud.available)
        {
            root.voiceNoticeShown = !root.voiceNoticeShown

            return
        }

        root.voiceNoticeShown = false
        textView.playReadAloud()
    }

    function handleReadAloudKey(event)
    {
        if (reader.isPdf || !reader.readAloud.started)
        {
            return false
        }

        if (event.key === Qt.Key_Space && event.modifiers === Qt.NoModifier)
        {
            if (reader.readAloud.playing)
            {
                reader.readAloud.pause()
            }
            else
            {
                textView.playReadAloud()
            }

            return true
        }

        if (!(event.modifiers & Qt.ControlModifier))
        {
            return false
        }

        if (event.key === Qt.Key_Left)
        {
            reader.readAloud.previous()

            return true
        }

        if (event.key === Qt.Key_Right)
        {
            reader.readAloud.next()

            return true
        }

        return false
    }

    function pickText(text, sentence)
    {
        pageArea.forceActiveFocus()
        translation.sentence = sentence || ""
        translation.text = text
    }

    function toggleImmersive()
    {
        root.immersive = !root.immersive
        keys.forceActiveFocus()
    }

    function dismiss()
    {
        if (translation.dismiss())
        {
            return true
        }

        if (root.contentsOpen)
        {
            root.contentsOpen = false

            return true
        }

        if (root.immersive)
        {
            root.immersive = false

            return true
        }

        return false
    }

    function onReadingKey(event)
    {
        if (bookMorph.morphing)
        {
            event.accepted = true

            return
        }

        if (!root.readable)
        {
            return
        }

        event.accepted = true

        if (textView.autoscrolling)
        {
            textView.stopAutoscroll()

            return
        }

        if (event.matches(StandardKey.Copy))
        {
            const selection = reader.isPdf ? pdfView.selectionText() : textView.selectionText()

            if (selection.length > 0)
            {
                translationLookup.copyToClipboard(selection)
            }

            return
        }

        if (root.handleReadAloudKey(event))
        {
            return
        }

        const zooming = event.modifiers & Qt.ControlModifier

        switch (event.key)
        {
        case Qt.Key_Left:
        case Qt.Key_PageUp:
            root.turnPage(false)
            return
        case Qt.Key_Right:
        case Qt.Key_PageDown:
        case Qt.Key_Space:
            root.turnPage(true)
            return
        case Qt.Key_Up:
            root.scrollBy(-AppMetrics.readerWheelStep)
            return
        case Qt.Key_Down:
            root.scrollBy(AppMetrics.readerWheelStep)
            return
        case Qt.Key_Home:
            root.goToPosition(0)
            return
        case Qt.Key_End:
            root.goToPosition(root.positionCount - 1)
            return
        case Qt.Key_Escape:
            event.accepted = root.dismiss()
            return
        case Qt.Key_Plus:
        case Qt.Key_Equal:
            if (event.modifiers & Qt.ControlModifier)
            {
                root.zoom(true)

                return
            }

            break
        case Qt.Key_Minus:
            if (event.modifiers & Qt.ControlModifier)
            {
                root.zoom(false)

                return
            }

            break
        case Qt.Key_0:
            if (zooming)
            {
                if (reader.isPdf)
                {
                    pdfView.fitWidth()
                }
                else
                {
                    AppReaderSettings.textSize = AppReaderSettings.defaultTextSize
                }

                return
            }

            break
        }

        event.accepted = false
    }

    onVisibleChanged:
    {
        if (root.visible)
        {
            reader.vocabulary.refresh()

            return
        }

        reader.readAloud.pause()
    }

    Connections
    {
        target: Qt.application

        function onStateChanged()
        {
            if (Qt.application.state === Qt.ApplicationActive && root.visible)
            {
                reader.vocabulary.refresh()
            }
        }
    }

    ReaderController
    {
        id: reader

        onCloudFailed: function(message) { root.messageRequested(message) }
        onFreeBookFailed: function(message) { root.messageRequested(message) }

        onCurrentDocumentChanged:
        {
            root.pdfPendingPage = reader.currentPosition
            root.pdfPendingZoom = reader.currentZoom
            root.pdfPendingScroll = reader.currentScroll
            root.voiceNoticeShown = false

            if (!reader.reading)
            {
                return
            }

            keys.forceActiveFocus()

            if (!reader.loading && !reader.isPdf)
            {
                Qt.callLater(function() { textView.goTo(reader.currentPosition) })
            }
        }
    }

    DictionaryController
    {
        id: translationLookup
    }

    Connections
    {
        target: reader.readAloud

        function onAvailableChanged()
        {
            if (!root.voiceNoticeShown || !reader.readAloud.available)
            {
                return
            }

            root.voiceNoticeShown = false
            textView.playReadAloud()
        }
    }

    Timer
    {
        id: positionSaveTimer

        interval: AppMetrics.readerSaveDelay

        onTriggered: root.savePosition()
    }

    Component.onCompleted: root.refresh()

    DesktopPage
    {
        id: library

        anchors.fill: parent
        visible: !root.reading || bookMorph.morphing
        opacity: bookMorph.shelfShown
        scale: bookMorph.shelfScale
        maximumWidth: AppMetrics.coverGridWidth
        spacing: AppMetrics.gapMedium

        RowLayout
        {
            Layout.fillWidth: true
            spacing: AppMetrics.gapTight

            DesktopSearchField
            {
                id: librarySearch

                objectName: "reader_search"

                Layout.preferredWidth: AppMetrics.searchWidth
                visible: root.shelfCount > 0
                placeholderText: qsTr("Search books")
            }

            DesktopLabel
            {
                Layout.fillWidth: true
                Layout.leftMargin: librarySearch.visible ? AppMetrics.gapSmall : 0
                visible: root.shelfCount > 0
                text: root.searchTerm.length > 0 || (root.freeShown && root.levelFilter.length > 0)
                    ? qsTr("%1 of %2 books").arg(root.shelf.length).arg(root.shelfCount)
                    : root.shelfCount === 1 ? qsTr("1 book") : qsTr("%1 books").arg(root.shelfCount)
                font.pixelSize: AppMetrics.fontSmall
                color: AppTheme.colourMuted
            }

            Item
            {
                Layout.fillWidth: true
                visible: root.shelfCount === 0
            }

            DesktopSelect
            {
                objectName: "reader_shelf"

                Layout.fillWidth: false
                Layout.preferredWidth: AppMetrics.settingsControlWidth
                visible: root.freeAvailable
                heading: qsTr("Show books")
                options: [qsTr("My books"), qsTr("Free books")]
                currentIndex: root.freeShown ? 1 : 0

                onChosen: function(index)
                {
                    root.freeShown = index === 1

                    if (root.freeShown)
                    {
                        reader.refreshFreeBooks()
                    }
                }
            }

            DesktopSelect
            {
                objectName: "reader_level"

                Layout.fillWidth: false
                Layout.preferredWidth: AppMetrics.settingsControlWidth
                visible: root.freeShown
                heading: qsTr("Reading level")
                options: Shelf.levelLabels()
                currentIndex: Shelf.levels.indexOf(root.levelFilter)

                onChosen: function(index) { root.levelFilter = Shelf.levels[index] }
            }

            DesktopSelect
            {
                objectName: "reader_sort"

                Layout.fillWidth: false
                Layout.preferredWidth: AppMetrics.settingsControlWidth
                visible: !root.freeShown
                enabled: root.bookCount > 1
                heading: qsTr("Sort by")
                options: [qsTr("Last opened"), qsTr("Recently added"), qsTr("Title"), qsTr("Progress")]
                currentIndex: Shelf.sortOrders.indexOf(root.sortOrder)

                onChosen: function(index) { AppReaderSettings.librarySortOrder = Shelf.sortOrders[index] }
            }

            DesktopIconButton
            {
                objectName: "reader_sort_direction"

                visible: !root.freeShown
                enabled: root.bookCount > 1
                iconName: root.sortReversed ? "arrow_up_narrow_wide" : "arrow_down_wide_narrow"
                text: root.sortDirections[Shelf.sortOrders.indexOf(root.sortOrder)][root.sortReversed ? 1 : 0]

                onClicked: AppReaderSettings.librarySortReversed = !root.sortReversed
            }
        }

        Flow
        {
            id: shelfFlow

            Layout.fillWidth: true
            spacing: AppMetrics.gapMedium

            Accessible.role: Accessible.List
            Accessible.name: qsTr("Books")

            DesktopReaderOpenTile
            {
                objectName: "reader_open_tile"

                width: root.tileWidth
                visible: !root.freeShown && root.searchTerm.length === 0

                onClicked: root.openBook()
            }

            Repeater
            {
                id: shelfRepeater

                model: root.shelf

                delegate: DesktopReaderBookTile
                {
                    id: tile

                    required property var modelData
                    required property int index

                    objectName: "reader_book_" + index

                    width: root.tileWidth

                    title: modelData.title
                    level: modelData.level || ""
                    format: modelData.format
                    exists: modelData.exists
                    local: modelData.local
                    inCloud: modelData.cloudId.length > 0
                    free: modelData.free || modelData.freeId !== undefined
                    cloudAvailable: reader.cloudAvailable
                    size: modelData.size || 0
                    readProgress: modelData.progress
                    progress:
                    {
                        const value = !modelData.local && modelData.freeId !== undefined
                                    ? reader.freeTransfers[modelData.freeId]
                                    : reader.transfers[modelData.local ? modelData.id : modelData.cloudId]

                        return value === undefined ? -1 : value
                    }

                    onClicked:
                    {
                        if (progress >= 0)
                        {
                            return
                        }

                        if (!modelData.local && modelData.freeId !== undefined)
                        {
                            reader.downloadFreeBook(modelData.freeId)

                            return
                        }

                        if (!modelData.local)
                        {
                            reader.downloadBook(modelData.cloudId)

                            return
                        }

                        if (!modelData.exists)
                        {
                            reader.locateDocument(modelData.id)

                            return
                        }

                        root.openFromTile(tile, modelData)
                    }
                    onShowRequested: reader.showOnDevice(modelData.id)
                    onRemoveRequested: root.confirmRemove(modelData)
                    onUploadRequested: reader.uploadDocument(modelData.id)
                    onCloudDeleteRequested: root.confirmCloudDelete(modelData)
                    onCancelRequested:
                    {
                        if (!modelData.local && modelData.freeId !== undefined)
                        {
                            reader.cancelFreeBook(modelData.freeId)

                            return
                        }

                        reader.cancelTransfer(modelData.local ? modelData.id : modelData.cloudId)
                    }
                }
            }
        }

        DesktopLabel
        {
            Layout.fillWidth: true
            visible: root.shelf.length === 0 && (root.searchTerm.length > 0 || (root.freeShown && root.levelFilter.length > 0))
            text: root.searchTerm.length > 0 ? qsTr("No books match \"%1\"").arg(root.searchTerm)
                                            : qsTr("No books at this level.")
            color: AppTheme.colourMuted
        }
    }

    DesktopConfirmDialog
    {
        id: cloudDeleteDialog
    }

    DropArea
    {
        id: dropTarget

        anchors.fill: parent
        enabled: !root.reading

        onDropped: function(drop)
        {
            if (!drop.hasUrls)
            {
                return
            }

            for (const url of drop.urls)
            {
                reader.openPath(url)
            }

            drop.accept()
        }
    }

    Rectangle
    {
        anchors.fill: parent
        anchors.margins: AppMetrics.gapMedium
        visible: dropTarget.containsDrag
        radius: AppMetrics.radiusLarge
        color: AppTheme.faded(AppTheme.colourAccent, AppTheme.alphaFaint)
        border.width: AppMetrics.focusRingWidth
        border.color: AppTheme.colourAccent
    }

    Item
    {
        id: readingView

        anchors.fill: parent
        visible: root.readable

        FocusScope
        {
            id: keys

            anchors.fill: parent

            Keys.onPressed: function(event) { root.onReadingKey(event) }

            DesktopReaderContentsPane
            {
                id: contentsPane

                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: bar.visible ? bar.top : parent.bottom
                z: 1
                visible: root.contentsOpen && !root.immersive

                entries: root.contentsEntries
                loading: reader.isPdf && pdfView.outlineLoading
                paged: reader.isPdf
                positionCount: root.positionCount

                onEntryActivated: function(position)
                {
                    root.goToPosition(position)
                    keys.forceActiveFocus()
                }
                onCloseRequested: root.contentsOpen = false
            }

            Rectangle
            {
                id: pageArea

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: bar.visible ? bar.top : parent.bottom

                color: AppTheme.faded(reader.isPdf ? AppTheme.colourSunken : root.textPaper, bookMorph.pageShown)

                PointHandler
                {
                    onActiveChanged: if (active) pageArea.forceActiveFocus()
                }

                DesktopReaderPdfView
                {
                    id: pdfView

                    objectName: "reader_pdf"

                    anchors.fill: parent
                    visible: reader.isPdf
                    opacity: bookMorph.chromeShown

                    source: reader.isPdf ? reader.currentPath : ""
                    initialPage: reader.currentPosition
                    initialZoom: reader.currentZoom
                    initialScroll: reader.currentScroll
                    cropMargins: reader.cropMargins
                    paperColour: root.pageTone.paper
                    inkColour: root.pageTone.ink

                    onWordPicked: function(word, sentence) { root.pickText(word, sentence) }
                    onTranslationWanted:
                    {
                        translation.expect()
                        Qt.callLater(translation.show)
                    }
                    onCurrentPageChanged: function(page)
                    {
                        root.pdfPendingPage = page
                        positionSaveTimer.restart()
                    }
                    onCurrentZoomChanged: function(zoom)
                    {
                        root.pdfPendingZoom = zoom
                        positionSaveTimer.restart()
                    }
                    onCurrentScrollChanged: function(scroll)
                    {
                        root.pdfPendingScroll = scroll
                        positionSaveTimer.restart()
                    }
                }

                DesktopReaderTextView
                {
                    id: textView

                    objectName: "reader_text"

                    anchors.fill: parent
                    visible: !reader.isPdf && !reader.loading
                    opacity: bookMorph.chromeShown
                    transform: Translate { y: AppMetrics.gapTight * (1 - bookMorph.chromeShown) }

                    model: reader.paragraphModel
                    textSize: reader.textSize
                    lineSpacing: reader.lineSpacing
                    textMargin: reader.textMargin
                    pageWidth: AppReaderSettings.pageWidth
                    fontFamily: AppReaderSettings.textFont.length > 0 ? AppReaderSettings.textFont : AppMetrics.fontFamily
                    inkColour: root.textInk
                    bookPath: reader.currentPath
                    vocabulary: reader.vocabulary
                    readAloud: reader.readAloud
                    readAloudInset: playBar.visible ? playBar.height + AppMetrics.gapMedium * 2 : 0

                    onWordPicked: function(word, sentence) { root.pickText(word, sentence) }
                    onTranslationWanted:
                    {
                        translation.expect()
                        Qt.callLater(translation.show)
                    }
                    onContentYChanged: positionSaveTimer.restart()
                    onZoomRequested: function(closer) { root.zoom(closer) }
                }

                DesktopReaderTranslation
                {
                    id: translation

                    anchors.fill: parent
                    visible: root.readable && !reader.loading
                    opacity: bookMorph.chromeShown

                    lookup: translationLookup
                    maxStage: root.store.stageCount
                    selecting: reader.isPdf ? pdfView.selecting : textView.selecting
                    darkPaper: !reader.isPdf && root.textPaper.hslLightness < 0.5
                    contentWidth: reader.isPdf ? Math.min(pageArea.width, pdfView.fitPoints * pdfView.renderScale) : textView.textWidth
                    geometry: reader.isPdf ? pdfView.selectionGeometry() : textView.selectionGeometry()
                    readAloudOffered: !reader.isPdf && reader.readAloud.available && textView.pickedPlace !== null

                    onAddWordRequested: function(word, translation, partOfSpeech, hint)
                    {
                        root.addWordRequested(word, translation, partOfSpeech, hint)
                    }
                    onDictionaryRequested: function(word) { root.lookUpRequested(word) }
                    onReadAloudRequested:
                    {
                        const place = textView.pickedPlace

                        translation.dismiss()
                        root.voiceNoticeShown = false
                        textView.readFrom(place)
                    }
                }

                Rectangle
                {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: playBar.top
                    anchors.bottomMargin: AppMetrics.gapSmall
                    visible: playBar.visible && textView.readingAway
                    opacity: bookMorph.chromeShown
                    implicitWidth: backToReading.implicitWidth
                    implicitHeight: backToReading.implicitHeight
                    radius: height / 2
                    color: AppTheme.colourCard
                    border.width: AppMetrics.borderWidth
                    border.color: AppTheme.colourBorder

                    DesktopButton
                    {
                        id: backToReading

                        objectName: "reader_read_aloud_back"

                        anchors.fill: parent
                        focusPolicy: Qt.NoFocus
                        tone: "ghost"
                        text: qsTr("Back to reading")

                        onClicked: textView.showReading()
                    }
                }

                DesktopReaderPlayBar
                {
                    id: playBar

                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: AppMetrics.gapMedium
                    visible: root.readAloudOffered && reader.readAloud.started && !reader.loading
                    opacity: bookMorph.chromeShown

                    readAloud: reader.readAloud

                    onPlayRequested: textView.playReadAloud()
                }

                DesktopReaderVoiceNotice
                {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: AppMetrics.gapMedium
                    visible: root.voiceNoticeShown && root.readAloudOffered && !reader.loading && !reader.readAloud.available
                    opacity: bookMorph.chromeShown

                    readAloud: reader.readAloud

                    onClosed: root.voiceNoticeShown = false
                }

                DesktopLabel
                {
                    anchors.centerIn: parent
                    visible: reader.loading
                    opacity: bookMorph.chromeShown
                    text: qsTr("Loading")
                    color: AppTheme.colourMuted
                }

                DesktopIconButton
                {
                    objectName: "reader_leave_immersive"

                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: AppMetrics.gapMedium
                    visible: root.immersive
                    iconName: "close"
                    text: qsTr("Show controls (Esc)")

                    onClicked: root.toggleImmersive()
                }
            }

            DesktopReaderBar
            {
                id: bar

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                visible: !root.immersive
                opacity: bookMorph.chromeShown
                transform: Translate { y: bar.height * (1 - bookMorph.chromeShown) }

                count: root.positionCount
                index: root.positionIndex
                paged: reader.isPdf
                fitting: pdfView.fitting

                onPositionRequested: function(index) { root.goToPosition(index) }
                onStepRequested: function(forward) { root.turnPage(forward) }
                onZoomRequested: function(closer) { root.zoom(closer) }
                onFitRequested: pdfView.fitWidth()
            }
        }
    }

    ColumnLayout
    {
        anchors.centerIn: parent
        width: Math.min(parent.width - AppMetrics.contentPadding * 2, AppMetrics.dialogWidth)
        visible: root.cannotRead
        spacing: AppMetrics.gapMedium

        DesktopLabel
        {
            Layout.fillWidth: true
            text: qsTr("Cannot read this book")
            font.pixelSize: AppMetrics.fontTitle
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
        }

        DesktopLabel
        {
            Layout.fillWidth: true
            text: qsTr("It may be corrupt, or protected in a way this reader cannot open.")
            color: AppTheme.colourMuted
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }

        RowLayout
        {
            Layout.alignment: Qt.AlignHCenter
            spacing: AppMetrics.gapSmall

            DesktopButton
            {
                objectName: "reader_remove_unreadable"

                text: qsTr("Remove from library")

                onClicked: root.removeCurrentBook()
            }

            DesktopButton
            {
                objectName: "reader_back_to_library"

                tone: "primary"
                text: qsTr("Back to library")

                onClicked: root.closeBook()
            }
        }
    }

    DesktopBookMorph
    {
        id: bookMorph

        anchors.fill: parent

        contentReady: root.cannotRead || (root.readable && !reader.loading && (!reader.isPdf || pdfView.pageDrawn))

        onOpenStarted: root.crumbsChanging()
    }
}
