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
    readonly property var continueBooks: Shelf.continueBooks(reader.documents)
    readonly property bool freeAvailable: reader.freeBooks.length > 0
    property string section: "all"
    property var filter: root.emptyFilter()
    readonly property real chromeShown: pageTransition.chromeShown
    readonly property ReaderReadAloud readAloud: reader.readAloud
    readonly property bool readAloudOffered: root.readable && !reader.isPdf
    property bool voiceNoticeShown: false

    property bool immersive: false
    readonly property string sortOrder: Shelf.libraryOrders.indexOf(AppReaderSettings.librarySortOrder) >= 0
                                        ? AppReaderSettings.librarySortOrder : Shelf.libraryOrders[0]
    readonly property bool sortReversed: AppReaderSettings.librarySortReversed
    readonly property string groupMode: Shelf.libraryGroups.indexOf(AppReaderSettings.libraryGroup) >= 0
                                        ? AppReaderSettings.libraryGroup : Shelf.libraryGroups[0]

    readonly property var sortDirections: ({
        "recent": [qsTr("Newest first"), qsTr("Oldest first")],
        "title": [qsTr("A to Z"), qsTr("Z to A")],
        "author": [qsTr("A to Z"), qsTr("Z to A")],
        "level": [qsTr("Easiest first"), qsTr("Hardest first")],
        "progress": [qsTr("In progress first"), qsTr("Finished first")],
        "added": [qsTr("Newest first"), qsTr("Oldest first")],
        "size": [qsTr("Largest first"), qsTr("Smallest first")]
    })

    readonly property string searchTerm: librarySearch.term
    readonly property var sourceBooks: root.section === "free" ? reader.freeBooks : reader.documents
    readonly property var shelf: Shelf.shelve(root.sourceBooks, root.section, root.filter, root.searchTerm,
                                              root.sortOrder, root.sortReversed)
    readonly property var shelfGroups: Shelf.group(root.shelf, root.groupMode)
    readonly property var shelfRows: Shelf.rows(root.shelfGroups, root.tileColumns)
    readonly property var formats: root.sourceBooks.map(function(book) { return (book.format || "").toLowerCase() })
                                                 .filter(function(format, index, all) { return format.length > 0 && all.indexOf(format) === index })
                                                 .sort()

    readonly property var sectionCounts:
    {
        const counts = { "free": reader.freeBooks.length }
        const keys = ["all", "reading", "finished", "favorites"]

        for (const collection of reader.collections)
        {
            keys.push("collection:" + collection.id)
        }

        for (const key of keys)
        {
            counts[key] = reader.documents.filter(function(book) { return Shelf.inSection(book, key) }).length
        }

        return counts
    }

    readonly property string sectionTitle:
    {
        const titles = {
            "all": qsTr("My books"),
            "reading": qsTr("Reading"),
            "finished": qsTr("Finished"),
            "favorites": qsTr("Favorites"),
            "free": qsTr("Free library")
        }

        if (titles[root.section] !== undefined)
        {
            return titles[root.section]
        }

        const collection = reader.collections.find(function(entry) { return "collection:" + entry.id === root.section })

        return collection ? collection.name : titles["all"]
    }

    readonly property real shelfColumnWidth: Math.max(0, Math.min(AppMetrics.coverGridWidth,
                                                                  shelfArea.width - AppMetrics.contentPadding * 2))
    readonly property int tileColumns: Math.max(1, Math.floor((root.shelfColumnWidth + AppMetrics.gapLarge)
                                                              / (AppMetrics.bookTileMinimumWidth + AppMetrics.gapLarge)))
    readonly property real tileWidth: Math.floor((root.shelfColumnWidth - AppMetrics.gapLarge * (root.tileColumns - 1))
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
        }
    }

    function emptyFilter()
    {
        return { "levels": [], "statuses": [], "storage": [], "formats": [] }
    }

    function toggleFilter(key, value)
    {
        const next = { "levels": root.filter.levels.slice(), "statuses": root.filter.statuses.slice(),
                       "storage": root.filter.storage.slice(), "formats": root.filter.formats.slice() }
        const index = next[key].indexOf(value)

        if (index >= 0)
        {
            next[key].splice(index, 1)
        }
        else
        {
            next[key].push(value)
        }

        root.filter = next
    }

    function download(book)
    {
        if (book.freeId !== undefined && !book.cloudId)
        {
            reader.downloadFreeBook(book.freeId)

            return
        }

        reader.downloadBook(book.cloudId)
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

    function recordAddedCard(cardId)
    {
        reader.recordAddedCard(cardId)
    }

    function openDocument(id)
    {
        root.savePosition()
        reader.openDocument(id)
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

    function openFromTile(document, position)
    {
        pageTransition.open()

        if (position === undefined)
        {
            reader.openDocument(document.id)
        }
        else
        {
            reader.openDocumentAt(document.id, position)
        }

        if (pageTransition.running && !reader.reading)
        {
            pageTransition.finishOpen()
        }
    }

    function closeBook()
    {
        root.contentsOpen = false
        root.immersive = false

        if (!root.readable || !pageTransition.close(root.closeNow))
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

        if (pageTransition.running)
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

    function readFromPicked()
    {
        const place = textView.pickedPlace

        translation.dismiss()
        root.voiceNoticeShown = false
        textView.readFrom(place)
    }

    function focusReading()
    {
        keys.forceActiveFocus()
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
            collections: reader.collections
            freeShown: root.freeAvailable

            onSectionChosen: function(section)
            {
                root.section = section

                if (section === "free")
                {
                    reader.refreshFreeBooks()
                }
            }
            onNewCollectionRequested: collectionDialog.create(function(name, colour) { reader.createCollection(name, colour) })
            onRenameCollectionRequested: function(collectionId, name)
            {
                collectionDialog.rename(collectionId, name, function(newName) { reader.renameCollection(collectionId, newName) })
            }
            onDeleteCollectionRequested: function(collectionId, name)
            {
                cloudDeleteDialog.ask(qsTr("Delete the collection \"%1\"?").arg(name),
                                      qsTr("The books stay in your library."),
                                      qsTr("Delete"),
                                      function() { reader.deleteCollection(collectionId) })
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
                y: AppMetrics.contentPadding
                width: root.shelfColumnWidth
                spacing: AppMetrics.gapSmall

                DesktopLabel
                {
                    Layout.minimumWidth: implicitWidth
                    text: root.sectionTitle
                    font.pixelSize: AppMetrics.fontTitle
                    font.weight: Font.DemiBold
                }

                DesktopLabel
                {
                    Layout.leftMargin: AppMetrics.gapTight
                    text: root.shelf.length === 1 ? qsTr("1 book") : qsTr("%1 books").arg(root.shelf.length)
                    font.pixelSize: AppMetrics.fontSmall
                    color: AppTheme.colourMuted
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

                            onClicked: root.filter = root.emptyFilter()
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
                                text: root.sortDirections[root.sortOrder][index]
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
                                cloudAvailable: reader.cloudAvailable
                                favorite: modelData.favorite || false
                                finished: modelData.finished || false
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

                                    if (!modelData.local)
                                    {
                                        root.download(modelData)

                                        return
                                    }

                                    if (!modelData.exists)
                                    {
                                        reader.locateDocument(modelData.id)

                                        return
                                    }

                                    root.openFromTile(modelData)
                                }
                                onRemoveRequested: root.confirmRemove(modelData)
                                onOverviewRequested: bookOverview.show(modelData.id)
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
                anchors.fill: parent
                anchors.margins: AppMetrics.gapMedium
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

        controller: reader
        maxStage: root.store.stageCount

        onReadRequested: function(book) { root.openFromTile(book) }
        onChapterRequested: function(book, position) { root.openFromTile(book, position) }
        onNewCollectionRequested: function(key)
        {
            collectionDialog.create(function(name, colour)
            {
                const collectionId = reader.createCollection(name, colour)

                if (collectionId.length > 0)
                {
                    reader.addToCollection(collectionId, key)
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
                reader.openPath(url)
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

                color: AppTheme.faded(reader.isPdf ? AppTheme.colourSunken : root.textPaper, pageTransition.pageShown)

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
                    opacity: pageTransition.chromeShown

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
                    opacity: pageTransition.chromeShown
                    transform: Translate { y: AppMetrics.gapTight * (1 - pageTransition.chromeShown) }

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
                    onReadAloudJumpRequested: root.readFromPicked()
                    onContentYChanged: positionSaveTimer.restart()
                    onZoomRequested: function(closer) { root.zoom(closer) }
                }

                DesktopReaderTranslation
                {
                    id: translation

                    anchors.fill: parent
                    visible: root.readable && !reader.loading
                    opacity: pageTransition.chromeShown

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
                    onReadAloudRequested: root.readFromPicked()
                }

                Rectangle
                {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: playBar.top
                    anchors.bottomMargin: AppMetrics.gapSmall
                    visible: playBar.visible && textView.readingAway
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
                    opacity: pageTransition.chromeShown

                    readAloud: reader.readAloud

                    onPlayRequested: textView.playReadAloud()
                }

                DesktopReaderVoiceNotice
                {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: AppMetrics.gapMedium
                    visible: root.voiceNoticeShown && root.readAloudOffered && !reader.loading && !reader.readAloud.available
                    opacity: pageTransition.chromeShown

                    readAloud: reader.readAloud

                    onClosed: root.voiceNoticeShown = false
                }

                DesktopLabel
                {
                    anchors.centerIn: parent
                    visible: reader.loading
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
                opacity: pageTransition.chromeShown
                transform: Translate { y: bar.height * (1 - pageTransition.chromeShown) }

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

    DesktopReaderTransition
    {
        id: pageTransition

        anchors.fill: parent

        contentReady: root.cannotRead || (root.readable && !reader.loading && (!reader.isPdf || pdfView.pageDrawn))

        onOpenStarted: root.crumbsChanging()
    }
}
