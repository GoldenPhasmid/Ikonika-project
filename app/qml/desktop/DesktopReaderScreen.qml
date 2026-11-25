import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../ReaderTones.js" as Tones
import "../ReaderShelf.js" as Shelf

Item
{
    id: root

    required property DictionaryController contextLookup
    required property DictionaryController textLookup
    required property DictionaryController lookup
    required property DictionaryController translationLookup
    required property var store
    required property ReaderController reader

    Binding
    {
        target: root.lookup
        property: "language"
        value: root.reader.bookLanguage.code
    }

    Binding
    {
        target: root.translationLookup
        property: "language"
        value: root.reader.bookLanguage.code
    }

    Binding
    {
        target: root.contextLookup
        property: "language"
        value: root.reader.bookLanguage.code
    }

    Binding
    {
        target: root.textLookup
        property: "language"
        value: root.reader.bookLanguage.code
    }

    signal addWordRequested(string word, string translation, string partOfSpeech, string hint)
    signal lookUpRequested(string word)
    signal messageRequested(string message)
    signal crumbsChanging()

    readonly property bool reading: root.reader.reading
    readonly property string currentTitle: root.reader.currentTitle
    readonly property bool isPdf: root.reader.isPdf
    readonly property bool cannotRead: root.reading && (root.reader.unreadable || (root.reader.isPdf && pdfView.failed))
    readonly property bool readable: root.reading && !root.cannotRead
    readonly property int bookCount: root.reader.documents.length
    readonly property var continueBooks: Shelf.continueBooks(root.reader.documents)
    readonly property bool freeAvailable: root.reader.freeBooks.length > 0
    property string section: "all"
    property var filter: Shelf.emptyFilter()
    readonly property real chromeShown: pageTransition.chromeShown
    readonly property ReaderReadAloud readAloud: root.reader.readAloud
    readonly property bool readAloudOffered: root.readable && (!root.reader.isPdf || root.reader.pdfNarration.available)
    property bool voiceNoticeShown: false

    property bool immersive: false
    readonly property real barSpace: bar.visible ? bar.height : 0
    property real topBarHeight: 0
    property bool topBarShown: true
    readonly property real topChrome: root.readable && root.topBarShown ? root.topBarHeight : 0
    readonly property string sortOrder: Shelf.libraryOrders.indexOf(AppReaderSettings.librarySortOrder) >= 0
                                        ? AppReaderSettings.librarySortOrder : Shelf.libraryOrders[0]
    readonly property bool sortReversed: AppReaderSettings.librarySortReversed
    readonly property string groupMode: Shelf.libraryGroups.indexOf(AppReaderSettings.libraryGroup) >= 0
                                        ? AppReaderSettings.libraryGroup : Shelf.libraryGroups[0]


    readonly property string searchTerm: librarySearch.term
    readonly property var sourceBooks: root.section === "free" ? root.reader.freeBooks : root.reader.documents
    readonly property var shelf: Shelf.shelve(root.sourceBooks, root.section, root.filter, root.searchTerm,
                                              root.sortOrder, root.sortReversed)
    readonly property var shelfGroups: Shelf.group(root.shelf, root.groupMode)
    readonly property var shelfRows: Shelf.rows(root.shelfGroups, root.tileColumns)
    readonly property var formats: Shelf.formats(root.sourceBooks)

    readonly property var sectionCounts: Shelf.sectionCounts(root.reader.documents, root.reader.freeBooks, root.reader.collections)

    readonly property string sectionTitle: Shelf.sectionTitle(root.section, root.reader.collections)

    readonly property real shelfColumnWidth: Math.max(0, Math.min(AppMetrics.coverGridWidth,
                                                                  shelfArea.width - AppMetrics.contentPadding * 2))
    readonly property int tileColumns: Math.max(1, Math.floor((root.shelfColumnWidth + AppMetrics.gapLarge)
                                                              / (AppMetrics.bookTileMinimumWidth + AppMetrics.gapLarge)))
    readonly property real tileWidth: Math.floor((root.shelfColumnWidth - AppMetrics.gapLarge * (root.tileColumns - 1))
                                                 / root.tileColumns)
    property bool contentsOpen: false
    property bool lookupOpen: false

    property int pdfPendingPage: 0
    property real pdfPendingZoom: 0
    property point pdfPendingScroll: Qt.point(0, 0)

    readonly property int positionCount: root.reader.isPdf ? pdfView.pageCount : textView.count
    readonly property int positionIndex: root.reader.isPdf ? pdfView.pageIndex : textView.topParagraph

    onPositionCountChanged:
    {
        if (root.readable && root.positionCount > 0)
        {
            root.reader.savePositionCount(root.positionCount)
        }
    }

    readonly property var contentsEntries: root.reader.isPdf ? pdfView.outlineEntries : root.reader.chapters
    readonly property var pageTone: Tones.find(root.reader.pageTone)
    readonly property var textTone: Tones.findText(AppReaderSettings.textTone)
    readonly property color textPaper: root.textTone.paper.length > 0 ? root.textTone.paper : AppTheme.colourPanel
    readonly property color textInk: root.textTone.ink.length > 0 ? root.textTone.ink : AppTheme.colourText

    function refresh()
    {
        root.reader.vocabulary.refresh()
        translation.refreshMatches()

        if (root.lookupOpen)
        {
            lookupPane.refresh()
        }

        if (!root.reader.reading)
        {
            root.reader.refresh()
        }
    }

    function toggleFilter(key, value)
    {
        root.filter = Shelf.toggledFilter(root.filter, key, value)
    }

    function download(book)
    {
        if (book.freeId !== undefined && !book.cloudId)
        {
            root.reader.downloadFreeBook(book.freeId)

            return
        }

        root.reader.downloadBook(book.cloudId)
    }

    function openBook()
    {
        root.reader.pickAndOpen()
    }

    function openPath(path)
    {
        root.savePosition()
        root.reader.openPath(path)
    }

    function recordAddedCard(cardId)
    {
        root.reader.recordAddedCard(cardId)
    }

    function openDocument(id)
    {
        root.savePosition()
        root.reader.openDocument(id)
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
                              function() { root.reader.removeDocument(document.id) })
    }

    function savePosition()
    {
        if (!root.reader.reading)
        {
            return
        }

        if (root.reader.isPdf)
        {
            root.reader.savePosition(root.pdfPendingPage)
            root.reader.saveZoom(root.pdfPendingZoom)
            root.reader.saveScroll(root.pdfPendingScroll)

            return
        }

        if (!root.reader.loading)
        {
            root.reader.savePosition(textView.topParagraph)
        }
    }

    function openFromTile(document, position)
    {
        pageTransition.open()

        if (position === undefined)
        {
            root.reader.openDocument(document.id)
        }
        else
        {
            root.reader.openDocumentAt(document.id, position)
        }

        if (pageTransition.running && !root.reader.reading)
        {
            pageTransition.finishOpen()
        }
    }

    function closeBook()
    {
        root.lookupOpen = false
        root.contentsOpen = false
        root.immersive = false

        if (pageTransition.opening)
        {
            pageTransition.finishOpen()
        }

        if (!root.readable || !pageTransition.close(root.closeNow))
        {
            root.closeNow()
        }
    }

    function closeNow()
    {
        if (pageTransition.opening)
        {
            pageTransition.finishOpen()
        }

        root.savePosition()

        root.lookupOpen = false
        root.contentsOpen = false
        root.immersive = false

        root.reader.closeDocument()
        root.reader.refresh()

        if (pageTransition.running)
        {
            root.crumbsChanging()
        }
    }

    function removeCurrentBook()
    {
        const id = root.reader.currentId

        root.closeNow()
        root.reader.removeDocument(id)
    }

    function goToPosition(index)
    {
        if (root.reader.isPdf)
        {
            pdfView.goToPage(index)

            return
        }

        textView.goTo(index)
    }

    function turnPage(forward)
    {
        if (root.reader.isPdf)
        {
            pdfView.goToPage(pdfView.pageIndex + (forward ? 1 : -1))

            return
        }

        textView.turnPage(forward)
    }

    function zoom(closer)
    {
        if (root.reader.isPdf)
        {
            pdfView.zoomBy(closer ? AppMetrics.readerZoomStep : 1 / AppMetrics.readerZoomStep)

            return
        }

        AppReaderSettings.textSize = AppReaderSettings.textSize + (closer ? 1 : -1)
    }

    function scrollBy(pixels)
    {
        if (root.reader.isPdf)
        {
            pdfView.scrollBy(pixels)

            return
        }

        textView.scrollBy(pixels)
    }

    function toggleContents()
    {
        root.contentsOpen = !root.contentsOpen

        if (root.contentsOpen && root.reader.isPdf && pdfView.outlineEntries.length === 0)
        {
            pdfView.requestOutline()
        }
    }

    function toggleReadAloud()
    {
        pageArea.forceActiveFocus()

        if (root.reader.readAloud.started)
        {
            root.reader.readAloud.stop()

            return
        }

        if (!root.reader.readAloud.available)
        {
            root.voiceNoticeShown = !root.voiceNoticeShown

            return
        }

        root.voiceNoticeShown = false
        root.playReadAloud()
    }

    function playReadAloud()
    {
        if (root.reader.isPdf)
        {
            pdfView.playReadAloud()

            return
        }

        textView.playReadAloud()
    }

    function readFromPicked()
    {
        const place = root.reader.isPdf ? pdfView.pickedPlace : textView.pickedPlace

        translation.dismiss()
        root.voiceNoticeShown = false

        if (root.reader.isPdf)
        {
            pdfView.readFrom(place)

            return
        }

        textView.readFrom(place)
    }

    function focusReading()
    {
        pageArea.forceActiveFocus()
    }

    function handleReadAloudKey(event)
    {
        if (!root.reader.readAloud.started)
        {
            return false
        }

        if (event.key === Qt.Key_Space && event.modifiers === Qt.NoModifier)
        {
            if (root.reader.readAloud.playing)
            {
                root.reader.readAloud.pause()
            }
            else
            {
                root.playReadAloud()
            }

            return true
        }

        if (!(event.modifiers & Qt.ControlModifier))
        {
            return false
        }

        if (event.key === Qt.Key_Left)
        {
            root.reader.readAloud.previous()

            return true
        }

        if (event.key === Qt.Key_Right)
        {
            root.reader.readAloud.next()

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

    function openLookup(term, focusField, sentence)
    {
        root.lookupOpen = true
        lookupPane.show(term, focusField, sentence)
    }

    function closeLookup()
    {
        root.lookupOpen = false
        pageArea.forceActiveFocus()
    }

    function toggleLookup()
    {
        if (root.lookupOpen)
        {
            root.closeLookup()

            return
        }

        root.openLookup("", true)
    }

    function toggleImmersive()
    {
        root.immersive = !root.immersive
        pageArea.forceActiveFocus()
    }

    function dismiss()
    {
        if (translation.dismiss())
        {
            return true
        }

        if (root.lookupOpen)
        {
            root.closeLookup()

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
        if (pageTransition.running)
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
            const selection = root.reader.isPdf ? pdfView.selectionText() : textView.selectionText()

            if (selection.length > 0)
            {
                root.translationLookup.copyToClipboard(selection)
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
                if (root.reader.isPdf)
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

    onLookupOpenChanged:
    {
        if (!root.lookupOpen && root.readable)
        {
            root.focusReading()
        }
    }

    onContentsOpenChanged:
    {
        if (!root.contentsOpen && root.readable)
        {
            root.focusReading()
        }
    }

    onVisibleChanged:
    {
        if (root.visible)
        {
            root.reader.vocabulary.refresh()

            return
        }

        root.reader.readAloud.pause()
    }

    Connections
    {
        target: Qt.application

        function onStateChanged()
        {
            if (Qt.application.state === Qt.ApplicationActive && root.visible)
            {
                root.reader.vocabulary.refresh()
            }
        }
    }

    Connections
    {
        target: root.reader

        function onCloudFailed(message) { root.messageRequested(message) }
        function onFreeBookFailed(message) { root.messageRequested(message) }
        function onCurrentDocumentChanged() { root.showCurrentDocument() }
        function onImportedDocumentOpening() { pageTransition.open() }
    }

    function showCurrentDocument()
    {
        root.pdfPendingPage = root.reader.currentPosition
        root.pdfPendingZoom = root.reader.currentZoom
        root.pdfPendingScroll = root.reader.currentScroll
        root.voiceNoticeShown = false

        if (!root.reader.reading)
        {
            return
        }

        pageArea.forceActiveFocus()

        if (!root.reader.loading && !root.reader.isPdf)
        {
            Qt.callLater(function() { textView.goTo(root.reader.currentPosition) })
        }
    }

    Connections
    {
        target: root.reader.readAloud

        function onAvailableChanged()
        {
            if (!root.voiceNoticeShown || !root.reader.readAloud.available)
            {
                return
            }

            root.voiceNoticeShown = false
            root.playReadAloud()
        }
    }

    Timer
    {
        id: positionSaveTimer

        interval: AppMetrics.readerSaveDelay

        onTriggered: root.savePosition()
    }

    Component.onCompleted:
    {
        root.refresh()
        root.showCurrentDocument()
    }

    Item
    {
        id: library

        anchors.fill: parent
        visible: !root.reading || pageTransition.running
        opacity: pageTransition.shelfShown
        scale: pageTransition.shelfScale

        DesktopReaderLibraryPane
        {
            id: libraryPane

            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom

            section: root.section
            counts: root.sectionCounts
            collections: root.reader.collections
            freeShown: root.freeAvailable

            onSectionChosen: function(section)
            {
                root.section = section

                if (section === "free")
                {
                    root.reader.refreshFreeBooks()
                }
            }
            onNewCollectionRequested: collectionDialog.create(function(name, colour) { root.reader.createCollection(name, colour) })
            onRenameCollectionRequested: function(collectionId, name)
            {
                collectionDialog.rename(collectionId, name, function(newName) { root.reader.renameCollection(collectionId, newName) })
            }
            onDeleteCollectionRequested: function(collectionId, name)
            {
                cloudDeleteDialog.ask(qsTr("Delete the collection \"%1\"?").arg(name),
                                      qsTr("The books stay in your library."),
                                      qsTr("Delete"),
                                      function() { root.reader.deleteCollection(collectionId) })
            }
            onImportRequested: root.openBook()
        }

        Item
        {
            id: shelfArea

            anchors.left: libraryPane.right
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom

            RowLayout
            {
                id: shelfToolbar

                x: (shelfArea.width - width) / 2
                y: AppMetrics.gapMedium
                width: root.shelfColumnWidth
                spacing: AppMetrics.gapSmall

                DesktopLabel
                {
                    Layout.minimumWidth: implicitWidth
                    text: root.sectionTitle
                    font.pixelSize: AppMetrics.fontTitle
                    font.weight: Font.DemiBold
                }

                Item
                {
                    Layout.fillWidth: true
                }

                DesktopSearchField
                {
                    id: librarySearch

                    objectName: "reader_search"

                    Layout.fillWidth: true
                    Layout.preferredWidth: AppMetrics.searchWidth
                    Layout.maximumWidth: AppMetrics.searchWidth
                    placeholderText: qsTr("Search title or author")
                }

                DesktopButton
                {
                    id: filterButton

                    objectName: "reader_filter"

                    iconName: "filter"
                    text: Shelf.filterCount(root.filter) > 0 ? qsTr("Filter · %1").arg(Shelf.filterCount(root.filter))
                                                             : qsTr("Filter")

                    onClicked: filterPopover.visible ? filterPopover.close() : filterPopover.open()

                    DesktopPopover
                    {
                        id: filterPopover

                        x: filterButton.width - width
                        y: filterButton.height + AppMetrics.gapTight
                        width: AppMetrics.popoverWidth
                        heading: qsTr("Filter")

                        component FilterGroup: ColumnLayout
                        {
                            id: filterGroup

                            required property string key
                            required property string title
                            required property var values
                            required property var labels

                            Layout.fillWidth: true
                            Layout.leftMargin: AppMetrics.gapCompact
                            Layout.rightMargin: AppMetrics.gapCompact
                            Layout.bottomMargin: AppMetrics.gapCompact
                            visible: filterGroup.values.length > 0
                            spacing: AppMetrics.gapSmall

                            DesktopLabel
                            {
                                text: filterGroup.title
                                font.pixelSize: AppMetrics.fontSmall
                                font.weight: Font.DemiBold
                                color: AppTheme.colourSubtle
                            }

                            Flow
                            {
                                Layout.fillWidth: true
                                spacing: AppMetrics.gapSmall

                                Repeater
                                {
                                    model: filterGroup.values

                                    delegate: DesktopChip
                                    {
                                        required property string modelData
                                        required property int index

                                        objectName: "reader_filter_" + filterGroup.key + "_" + modelData
                                        text: filterGroup.labels[index]
                                        toggle: true
                                        selected: root.filter[filterGroup.key].indexOf(modelData) >= 0

                                        onClicked: root.toggleFilter(filterGroup.key, modelData)
                                    }
                                }
                            }
                        }

                        FilterGroup
                        {
                            key: "levels"
                            title: qsTr("Level")
                            values: Shelf.levels.slice(1)
                            labels: Shelf.levelLabels().slice(1)
                        }

                        FilterGroup
                        {
                            key: "statuses"
                            title: qsTr("Status")
                            values: ["new", "reading", "finished"]
                            labels: [qsTr("Not started"), qsTr("Reading"), qsTr("Finished")]
                        }

                        FilterGroup
                        {
                            key: "storage"
                            title: qsTr("Storage")
                            values: ["downloaded", "cloud"]
                            labels: [qsTr("Downloaded"), qsTr("In the cloud")]
                        }

                        FilterGroup
                        {
                            key: "formats"
                            title: qsTr("Format")
                            values: root.formats
                            labels: root.formats.map(function(format) { return format.toUpperCase() })
                        }

                        DesktopButton
                        {
                            objectName: "reader_filter_reset"

                            Layout.alignment: Qt.AlignRight
                            Layout.margins: AppMetrics.gapCompact
                            enabled: Shelf.filterCount(root.filter) > 0
                            text: qsTr("Reset")

                            onClicked: root.filter = Shelf.emptyFilter()
                        }
                    }
                }

                DesktopButton
                {
                    id: sortButton

                    objectName: "reader_sort"

                    iconName: root.sortReversed ? "arrow_up_narrow_wide" : "arrow_down_wide_narrow"
                    trailingIconName: "chevron_down"
                    text: Shelf.libraryOrderLabels()[Shelf.libraryOrders.indexOf(root.sortOrder)]

                    onClicked: sortPopover.visible ? sortPopover.close() : sortPopover.open()

                    DesktopPopover
                    {
                        id: sortPopover

                        x: sortButton.width - width
                        y: sortButton.height + AppMetrics.gapTight
                        heading: qsTr("Sort by")

                        Repeater
                        {
                            model: Shelf.libraryOrders

                            delegate: DesktopPopoverRow
                            {
                                required property string modelData
                                required property int index

                                objectName: "reader_sort_" + modelData
                                Layout.fillWidth: true
                                text: Shelf.libraryOrderLabels()[index]
                                showsCheck: true
                                selected: root.sortOrder === modelData

                                onClicked: AppReaderSettings.librarySortOrder = modelData
                            }
                        }

                        DesktopDivider
                        {
                            Layout.fillWidth: true
                            Layout.topMargin: AppMetrics.gapTight
                            Layout.bottomMargin: AppMetrics.gapTight
                        }

                        Repeater
                        {
                            model: 2

                            delegate: DesktopPopoverRow
                            {
                                required property int index

                                objectName: "reader_sort_direction_" + index
                                Layout.fillWidth: true
                                text: Shelf.libraryOrderDirections()[root.sortOrder][index]
                                showsCheck: true
                                selected: root.sortReversed === (index === 1)

                                onClicked: AppReaderSettings.librarySortReversed = index === 1
                            }
                        }

                        DesktopDivider
                        {
                            Layout.fillWidth: true
                            Layout.topMargin: AppMetrics.gapTight
                            Layout.bottomMargin: AppMetrics.gapTight
                        }

                        DesktopSectionLabel
                        {
                            Layout.leftMargin: AppMetrics.gapCompact
                            Layout.bottomMargin: AppMetrics.gapTight
                            text: qsTr("Group by")
                        }

                        Repeater
                        {
                            model: Shelf.libraryGroups

                            delegate: DesktopPopoverRow
                            {
                                required property string modelData
                                required property int index

                                objectName: "reader_group_" + modelData
                                Layout.fillWidth: true
                                text: Shelf.libraryGroupLabels()[index]
                                showsCheck: true
                                selected: root.groupMode === modelData

                                onClicked: AppReaderSettings.libraryGroup = modelData
                            }
                        }
                    }
                }
            }

            ListView
            {
                id: shelfList

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: shelfToolbar.bottom
                anchors.topMargin: AppMetrics.gapMedium
                anchors.bottom: parent.bottom
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: shelfList.rows

                property var rows: []
                property string rowsIdentity: ""

                function adoptRows()
                {
                    const identity = [root.section, root.searchTerm, root.sortOrder, root.sortReversed, root.groupMode,
                                      root.tileColumns, JSON.stringify(root.filter)].join("|")
                    const heldPosition = shelfList.contentY
                    const keepPosition = identity === shelfList.rowsIdentity

                    shelfList.rowsIdentity = identity
                    shelfList.rows = root.shelfRows

                    if (keepPosition)
                    {
                        shelfList.contentY = heldPosition
                    }
                }

                Component.onCompleted: shelfList.adoptRows()

                Connections
                {
                    target: root

                    function onShelfRowsChanged() { shelfList.adoptRows() }
                }

                Accessible.role: Accessible.List
                Accessible.name: root.sectionTitle

                ScrollBar.vertical: DesktopScrollBar {}

                header: Item
                {
                    width: shelfList.width
                    height: AppMetrics.gapSmall
                }

                footer: Item
                {
                    width: shelfList.width
                    height: AppMetrics.contentPadding
                }

                delegate: Item
                {
                    id: shelfRow

                    required property var modelData
                    required property int index

                    width: shelfList.width
                    height: shelfRow.modelData.title !== undefined
                            ? groupLabel.implicitHeight + AppMetrics.gapMedium
                            : tileRow.implicitHeight + AppMetrics.gapLarge

                    DesktopSectionLabel
                    {
                        id: groupLabel

                        x: (shelfRow.width - root.shelfColumnWidth) / 2
                        visible: shelfRow.modelData.title !== undefined
                        text: shelfRow.modelData.title || ""
                    }

                    Row
                    {
                        id: tileRow

                        x: (shelfRow.width - root.shelfColumnWidth) / 2
                        spacing: AppMetrics.gapLarge

                        Repeater
                        {
                            id: tileRepeater

                            model: shelfRow.modelData.books || []

                            delegate: DesktopReaderBookTile
                            {
                                id: tile

                                required property var modelData
                                required property int index

                                objectName: "reader_book_" + (shelfRow.modelData.first + index)

                                width: root.tileWidth

                                title: modelData.title
                                author: modelData.author || ""
                                level: modelData.level || ""
                                format: modelData.format
                                coverSource: modelData.cover || ""
                                exists: modelData.exists
                                local: modelData.local
                                free: modelData.free || modelData.freeId !== undefined
                                catalogEntry: !modelData.local && modelData.freeId !== undefined
                                cloudAvailable: root.reader.cloudAvailable
                                favorite: modelData.favorite || false
                                finished: modelData.finished || false
                                size: modelData.size || 0
                                readProgress: modelData.progress
                                progress:
                                {
                                    const value = !modelData.local && modelData.freeId !== undefined
                                                ? root.reader.freeTransfers[modelData.freeId]
                                                : root.reader.transfers[modelData.local ? modelData.id : modelData.cloudId]

                                    return value === undefined ? -1 : value
                                }

                                onClicked:
                                {
                                    if (progress >= 0)
                                    {
                                        return
                                    }

                                    if (!modelData.local)
                                    {
                                        root.download(modelData)

                                        return
                                    }

                                    if (!modelData.exists)
                                    {
                                        root.reader.locateDocument(modelData.id)

                                        return
                                    }

                                    root.openFromTile(modelData)
                                }
                                onRemoveRequested: root.confirmRemove(modelData)
                                onFavoriteToggled: root.reader.setFavorite(modelData.key, !tile.favorite)
                                onOverviewRequested: bookOverview.show(modelData.id)
                                onCancelRequested:
                                {
                                    if (!modelData.local && modelData.freeId !== undefined)
                                    {
                                        root.reader.cancelFreeBook(modelData.freeId)

                                        return
                                    }

                                    root.reader.cancelTransfer(modelData.local ? modelData.id : modelData.cloudId)
                                }
                            }
                        }
                    }
                }
            }

            DesktopLabel
            {
                x: (shelfArea.width - width) / 2
                anchors.top: shelfToolbar.bottom
                anchors.topMargin: AppMetrics.gapLarge
                width: root.shelfColumnWidth
                visible: root.shelf.length === 0
                text: root.searchTerm.length > 0 ? qsTr("No books match \"%1\"").arg(root.searchTerm)
                    : Shelf.filterCount(root.filter) > 0 ? qsTr("No books match the filter.")
                    : qsTr("No books here yet.")
                color: AppTheme.colourMuted
            }

            Rectangle
            {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: shelfToolbar.bottom
                anchors.bottom: parent.bottom
                anchors.leftMargin: AppMetrics.gapMedium
                anchors.rightMargin: AppMetrics.gapMedium
                anchors.bottomMargin: AppMetrics.gapMedium
                visible: dropTarget.containsDrag
                radius: AppMetrics.radiusLarge
                color: AppTheme.faded(AppTheme.colourAccent, AppTheme.alphaFaint)
                border.width: AppMetrics.focusRingWidth
                border.color: AppTheme.colourAccent

                DesktopIcon
                {
                    anchors.centerIn: parent
                    name: "plus"
                    color: AppTheme.colourAccent
                    width: AppMetrics.readerOpenPlusSize
                    height: AppMetrics.readerOpenPlusSize
                }
            }
        }
    }

    DesktopCollectionDialog
    {
        id: collectionDialog
    }

    DesktopBookOverviewDialog
    {
        id: bookOverview

        controller: root.reader
        maxStage: root.store.stageCount

        onReadRequested: function(book) { root.openFromTile(book) }
        onChapterRequested: function(book, position) { root.openFromTile(book, position) }
        onRemoveRequested: function(book) { root.confirmRemove(book) }
        onCloudDeleteRequested: function(book)
        {
            cloudDeleteDialog.ask(qsTr("Delete \"%1\" from the cloud?").arg(book.title),
                                  qsTr("Other devices can no longer download it."),
                                  qsTr("Delete"),
                                  function() { root.reader.deleteCloudBook(book.cloudId) })
        }
        onNewCollectionRequested: function(key)
        {
            collectionDialog.create(function(name, colour)
            {
                const collectionId = root.reader.createCollection(name, colour)

                if (collectionId.length > 0)
                {
                    root.reader.addToCollection(collectionId, key)
                }
            })
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
                root.reader.openPath(url)
            }

            drop.accept()
        }
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
                anchors.topMargin: root.topChrome
                anchors.bottom: bar.visible ? bar.top : parent.bottom
                z: 1
                visible: root.contentsOpen && !root.immersive

                entries: root.contentsEntries
                loading: root.reader.isPdf && pdfView.outlineLoading
                paged: root.reader.isPdf
                positionCount: root.positionCount

                onEntryActivated: function(position)
                {
                    root.goToPosition(position)
                    pageArea.forceActiveFocus()
                }
                onCloseRequested: root.contentsOpen = false
            }

            DesktopLookupPane
            {
                id: lookupPane

                anchors.right: parent.right
                anchors.top: parent.top
                anchors.topMargin: root.topChrome
                anchors.bottom: bar.visible ? bar.top : parent.bottom
                z: 1
                visible: root.lookupOpen

                lookup: root.lookup
                maxStage: root.store.stageCount

                onAddWordRequested: function(word, translation, partOfSpeech, hint)
                {
                    root.addWordRequested(word, translation, partOfSpeech, hint)
                }
            }

            Rectangle
            {
                id: pageArea

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom

                color: AppTheme.faded(root.reader.isPdf ? AppTheme.colourSunken : root.textPaper, pageTransition.pageShown)

                PointHandler
                {
                    onActiveChanged: if (active) pageArea.forceActiveFocus()
                }

                DesktopReaderPdfView
                {
                    id: pdfView

                    objectName: "reader_pdf"

                    anchors.fill: parent
                    visible: root.reader.isPdf
                    opacity: pageTransition.chromeShown

                    source: root.reader.isPdf ? root.reader.currentPath : ""
                    initialPage: root.reader.currentPosition
                    initialZoom: root.reader.currentZoom
                    initialScroll: root.reader.currentScroll
                    cropMargins: root.reader.cropMargins
                    startPadding: AppMetrics.readerEndSpace + root.topBarHeight
                    endPadding: AppMetrics.readerEndSpace
                    bottomChrome: root.barSpace
                    paperColour: root.pageTone.paper
                    inkColour: root.pageTone.ink
                    readAloud: root.reader.readAloud
                    narration: root.reader.pdfNarration
                    readAloudInset: (playBar.visible ? playBar.height + AppMetrics.gapMedium * 2 : 0) + root.barSpace
                    topChrome: root.topChrome

                    onWordPicked: function(word, sentence)
                    {
                        if (root.reader.readAloud.playing)
                        {
                            root.reader.readAloud.pause()
                        }

                        root.pickText(word, sentence)
                    }
                    onTranslationWanted:
                    {
                        translation.expect()
                        Qt.callLater(translation.show)
                    }
                    onReadAloudJumpRequested: root.readFromPicked()
                    onDoubleTapped: root.closeLookup()
                    onBlankTapped: root.toggleImmersive()
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

                    contextLookup: root.textLookup

                    objectName: "reader_text"

                    anchors.fill: parent
                    visible: !root.reader.isPdf && !root.reader.loading
                    opacity: pageTransition.chromeShown
                    transform: Translate { y: AppMetrics.gapTight * (1 - pageTransition.chromeShown) }

                    model: root.reader.paragraphModel
                    textSize: root.reader.textSize
                    lineSpacing: root.reader.lineSpacing
                    textMargin: root.reader.textMargin
                    pageWidth: AppReaderSettings.pageWidth
                    fontFamily: AppReaderSettings.textFont.length > 0 ? AppReaderSettings.textFont : AppMetrics.fontFamily
                    inkColour: root.textInk
                    bookPath: root.reader.currentPath
                    vocabulary: root.reader.vocabulary
                    readAloud: root.reader.readAloud
                    readAloudInset: (playBar.visible ? playBar.height + AppMetrics.gapMedium * 2 : 0) + root.barSpace
                    bottomChrome: root.barSpace
                    topReserve: root.topBarHeight
                    topChrome: root.topChrome

                    onWordPicked: function(word, sentence) { root.pickText(word, sentence) }
                    onTranslationWanted:
                    {
                        translation.expect()
                        Qt.callLater(translation.show)
                    }
                    onReadAloudJumpRequested: root.readFromPicked()
                    onBlankTapped: root.toggleImmersive()
                    onContentYChanged: positionSaveTimer.restart()
                    onZoomRequested: function(closer) { root.zoom(closer) }
                }

                DesktopReaderTranslation
                {
                    id: translation

                    contextLookup: root.contextLookup

                    anchors.fill: parent
                    anchors.rightMargin: lookupPane.visible ? lookupPane.width : 0
                    anchors.bottomMargin: root.barSpace
                    visible: root.readable && !root.reader.loading
                    opacity: pageTransition.chromeShown

                    lookup: root.translationLookup
                    maxStage: root.store.stageCount
                    selecting: root.reader.isPdf ? pdfView.selecting : textView.selecting
                    darkPaper: !root.reader.isPdf && root.textPaper.hslLightness < 0.5
                    contentWidth: root.reader.isPdf ? Math.min(pageArea.width, pdfView.fitPoints * pdfView.renderScale) : textView.textWidth
                    geometry: root.reader.isPdf ? pdfView.selectionGeometry() : textView.selectionGeometry()
                    readAloudOffered: root.reader.readAloud.available
                                      && (root.reader.isPdf ? pdfView.narrated && pdfView.pickedPlace !== null : textView.pickedPlace !== null)
                    knownOffered: !root.reader.isPdf

                    onAddWordRequested: function(word, translation, partOfSpeech, hint)
                    {
                        root.addWordRequested(word, translation, partOfSpeech, hint)
                    }
                    onDictionaryRequested: function(word) { root.lookUpRequested(word) }
                    onReadAloudRequested: root.readFromPicked()
                    onKnownMarkChanged: root.reader.vocabulary.refresh()
                }

                Rectangle
                {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: playBar.top
                    anchors.bottomMargin: AppMetrics.gapSmall
                    visible: playBar.visible && (root.reader.isPdf ? pdfView.readingAway : textView.readingAway)
                    opacity: pageTransition.chromeShown
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

                        onClicked: root.reader.isPdf ? pdfView.showReading() : textView.showReading()
                    }
                }

                DesktopReaderPlayBar
                {
                    id: playBar

                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: AppMetrics.gapMedium + root.barSpace
                    visible: root.readAloudOffered && root.reader.readAloud.started && !root.reader.loading
                    opacity: pageTransition.chromeShown

                    readAloud: root.reader.readAloud

                    onPlayRequested: root.playReadAloud()
                }

                DesktopReaderVoiceNotice
                {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: AppMetrics.gapMedium + root.barSpace
                    visible: root.voiceNoticeShown && root.readAloudOffered && !root.reader.loading && !root.reader.readAloud.available
                    opacity: pageTransition.chromeShown

                    readAloud: root.reader.readAloud

                    onClosed: root.voiceNoticeShown = false
                }

                DesktopLabel
                {
                    anchors.centerIn: parent
                    visible: root.reader.loading
                    opacity: pageTransition.chromeShown
                    text: qsTr("Loading")
                    color: AppTheme.colourMuted
                }

                DesktopIconButton
                {
                    objectName: "reader_leave_immersive"

                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: AppMetrics.gapMedium
                    anchors.rightMargin: AppMetrics.gapMedium + (lookupPane.visible ? lookupPane.width : 0)
                    visible: root.immersive
                    iconName: "close"
                    iconColour: root.reader.isPdf ? AppTheme.colourText : root.textInk
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
                opacity: pageTransition.chromeShown
                transform: Translate { y: bar.height * (1 - pageTransition.chromeShown) }

                count: root.positionCount
                index: root.positionIndex
                paged: root.reader.isPdf
                fitting: pdfView.fitting

                onPositionRequested: function(index) { root.goToPosition(index) }
                onStepRequested: function(forward) { root.turnPage(forward) }
                onZoomRequested: function(closer) { root.zoom(closer) }
                onFitRequested: pdfView.fitWidth()
                onImmersiveRequested: root.toggleImmersive()
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

    DesktopReaderTransition
    {
        id: pageTransition

        anchors.fill: parent

        contentReady: root.cannotRead || (root.readable && !root.reader.loading && (!root.reader.isPdf || pdfView.pageDrawn))

        onOpenStarted: root.crumbsChanging()
    }
}
