import QtQuick
import QtQuick.Layouts
import "../ReaderTones.js" as Tones
import "../ReaderShelf.js" as Shelf
import "../ReadAloudPlace.js" as ReadAloudPlace

Item
{
    id: root

    required property DictionaryController lookupDictionary
    required property DictionaryController translationDictionary
    required property DictionaryController contextDictionary
    required property ReaderController reader

    Binding
    {
        target: root.lookupDictionary
        property: "language"
        value: root.reader.bookLanguage.code
    }

    Binding
    {
        target: root.translationDictionary
        property: "language"
        value: root.reader.bookLanguage.code
    }

    Binding
    {
        target: root.contextDictionary
        property: "language"
        value: root.reader.bookLanguage.code
    }

    required property int stageCount

    signal tabSelected(string key)
    signal addWordRequested(string word, string translation, string partOfSpeech, string hint)
    signal entryOpened(string headword, string partOfSpeech, string hint)

    readonly property bool reading: root.reader.reading

    readonly property bool popupLookup: AppMetrics.tabletScreen
    readonly property bool lookupOpen: lookupSheet.shown || translationPopup.open
    property bool outlineOpen: false
    property bool settingsOpen: false
    property bool voiceNoticeShown: false
    property var voiceNoticePlace: null
    property var lookupPlace: null
    readonly property bool readAloudOffered: !root.reader.isPdf || root.reader.pdfNarration.available

    property int pdfPendingPage: 0
    property real pdfPendingZoom: 0
    property point pdfPendingScroll: Qt.point(0, 0)

    property bool searchOpen: false
    property string section: "all"
    property var filter: Shelf.emptyFilter()
    readonly property string sortOrder: Shelf.libraryOrders.indexOf(AppReaderSettings.librarySortOrder) >= 0
                                        ? AppReaderSettings.librarySortOrder : Shelf.libraryOrders[0]
    readonly property bool sortReversed: AppReaderSettings.librarySortReversed
    readonly property string groupMode: Shelf.libraryGroups.indexOf(AppReaderSettings.libraryGroup) >= 0
                                        ? AppReaderSettings.libraryGroup : Shelf.libraryGroups[0]
    readonly property var books: root.reader.documents.filter(document => !document.local || document.exists)
    readonly property bool freeAvailable: root.reader.freeBooks.length > 0
    readonly property string searchTerm: (libraryShell.searchFieldText + libraryShell.searchFieldPreeditText).trim()
    readonly property var sourceBooks: root.section === "free" ? root.reader.freeBooks : root.books
    readonly property var shelf: Shelf.shelve(root.sourceBooks, root.section, root.filter, root.searchTerm,
                                              root.sortOrder, root.sortReversed)
    readonly property var shelfGroups: Shelf.group(root.shelf, root.groupMode)
    property var shownGroups: []
    property string shownSignature: ""

    onShelfGroupsChanged: root.adoptShelf()
    readonly property string sectionTitle: Shelf.sectionTitle(root.section, root.reader.collections)

    onFreeAvailableChanged:
    {
        if (!root.freeAvailable && root.section === "free")
        {
            root.section = "all"
        }
        else if (root.freeAvailable && root.books.length === 0)
        {
            root.section = "free"
        }
    }
    readonly property bool listLayout: AppViewLayouts.revision >= 0 && AppViewLayouts.isList("reader")

    readonly property int topParagraph: Math.max(0, readingList.indexAt(1, readingList.contentY + 1))
    readonly property bool cannotRead: root.reading
        && (root.reader.unreadable || (root.reader.isPdf && pdfView.failed))
    readonly property bool chromeVisible: !root.reader.fullScreen

    readonly property int positionCount: root.reader.isPdf ? pdfView.pageCount : readingList.count
    readonly property int positionIndex: root.reader.isPdf ? pdfView.pageIndex : root.topParagraph
    readonly property int loadedPositionCount: root.reading && !root.cannotRead && !root.reader.loading
        && (!root.reader.isPdf || pdfView.ready) ? root.positionCount : 0

    onLoadedPositionCountChanged:
    {
        if (root.loadedPositionCount > 0)
        {
            root.reader.savePositionCount(root.loadedPositionCount)
        }
    }

    readonly property var contentsEntries: root.reader.isPdf ? pdfView.outlineEntries : root.reader.chapters

    readonly property var pageTone: Tones.find(root.reader.pageTone)
    readonly property var textTone: Tones.findText(AppReaderSettings.textTone)
    readonly property bool textToned: root.textTone.paper.length > 0
    readonly property color textInk: root.textToned ? root.textTone.ink : AppTheme.colourText
    readonly property string textFont: AppReaderSettings.textFont.length > 0 ? AppReaderSettings.textFont
                                                                             : AppMetrics.fontFamily
    readonly property bool contentsLoading: root.reader.isPdf && pdfView.outlineLoading

    function percentOf(index)
    {
        return root.positionCount > 1 ? Math.round(100 * index / (root.positionCount - 1)) : 0
    }

    function goToPosition(index)
    {
        if (root.reader.isPdf)
        {
            pdfView.goToPage(index)

            return
        }

        readingList.leaveReadAloud()
        readingList.positionViewAtIndex(Math.max(0, Math.min(readingList.count - 1, index)),
                                        ListView.Beginning)
        positionSaveTimer.restart()
    }

    function openShelfBook(document, transferring)
    {
        if (transferring)
        {
            return
        }

        if (!document.local && document.freeId !== undefined)
        {
            root.reader.downloadFreeBook(document.freeId)

            return
        }

        if (!document.local)
        {
            root.reader.downloadBook(document.cloudId)

            return
        }

        root.reader.openDocument(document.id)
    }

    function adoptShelf()
    {
        const signature = JSON.stringify(root.shelfGroups)

        if (signature !== root.shownSignature)
        {
            root.shownSignature = signature
            root.shownGroups = root.shelfGroups
        }
    }

    function recordAddedCard(cardId)
    {
        root.reader.recordAddedCard(cardId)
    }

    function chooseSection(section)
    {
        root.section = section

        if (section === "free")
        {
            root.reader.refreshFreeBooks()
        }
    }

    function showCollections(book)
    {
        const collections = root.reader.collections
        const member = book.collections || []

        levelPicker.open(qsTr("Collections"),
                         collections.map(function(collection) { return collection.name }).concat([qsTr("New collection…")]),
                         -1,
                         function(index)
                         {
                             if (index >= collections.length)
                             {
                                 root.createCollection(book.key)

                                 return
                             }

                             const collectionId = collections[index].id

                             if (member.indexOf(collectionId) >= 0)
                             {
                                 root.reader.removeFromCollection(collectionId, book.key)

                                 return
                             }

                             root.reader.addToCollection(collectionId, book.key)
                         },
                         [],
                         collections.map(function(collection) { return member.indexOf(collection.id) >= 0 ? "✓" : "" }).concat([""]))
    }

    function createCollection(bookKey)
    {
        nameSheet.open(qsTr("New collection"), "", qsTr("Collection name"), true,
                       root.reader.collections.length % AppTheme.coverColours.length,
                       function(name, colour)
                       {
                           const collectionId = root.reader.createCollection(name, colour)

                           if (collectionId.length > 0 && bookKey.length > 0)
                           {
                               root.reader.addToCollection(collectionId, bookKey)
                           }
                       })
    }

    function showCollectionActions(collectionId, name)
    {
        levelPicker.open(name, [qsTr("Rename"), qsTr("Delete collection")], -1, function(index)
        {
            if (index === 0)
            {
                nameSheet.open(qsTr("Rename collection"), name, qsTr("Collection name"), false, 0,
                               function(newName) { root.reader.renameCollection(collectionId, newName) })

                return
            }

            confirmSheet.ask(qsTr("Delete the collection \"%1\"?").arg(name),
                             qsTr("The books stay in your library."),
                             qsTr("Delete"),
                             function()
                             {
                                 if (root.section === "collection:" + collectionId)
                                 {
                                     root.section = "all"
                                 }

                                 root.reader.deleteCollection(collectionId)
                             })
        })
    }

    function renameBook()
    {
        const book = bookPage.book

        if (book === null)
        {
            return
        }

        nameSheet.open(qsTr("Rename book"), book.title, qsTr("Title"), false, 0, function(title)
        {
            if (title !== book.title)
            {
                root.reader.renameDocument(book.id, title)
            }
        })
    }

    function openBookAt(book, position)
    {
        bookPage.close()

        if (position === undefined)
        {
            root.reader.openDocument(book.id)

            return
        }

        root.reader.openDocumentAt(book.id, position)
    }

    function openExternal(path, name)
    {
        if (root.reader.reading)
        {
            root.closeBook()
        }

        root.reader.openExternal(path, name)
    }

    function turnPage(forward)
    {
        if (root.reader.isPdf)
        {
            pdfView.goToPage(pdfView.pageIndex + (forward ? 1 : -1))

            return
        }

        readingList.leaveReadAloud()

        const step = readingList.height * 0.9 * (forward ? 1 : -1)
        const top = readingList.originY
        const bottom = Math.max(top, readingList.originY + readingList.contentHeight - readingList.height)

        readingList.contentY = Math.max(top, Math.min(bottom, readingList.contentY + step))
    }

    function toggleChrome()
    {
        if (translationPopup.dismiss())
        {
            return
        }

        root.reader.fullScreen = !root.reader.fullScreen
    }

    function openLookup(term, focusField, sentence, place)
    {
        root.lookupPlace = place || null

        const asPopup = root.popupLookup && !root.reader.isPdf && !focusField && root.lookupPlace !== null

        console.info("Reader lookup opens as", asPopup ? "popup" : "sheet")

        if (asPopup)
        {
            translationPopup.openFor(term, sentence)

            return
        }

        lookupSheet.openFor(term, focusField, sentence)
    }

    function toggleReadAloud()
    {
        if (root.reader.readAloud.started)
        {
            root.reader.readAloud.stop()

            return
        }

        if (!root.reader.readAloud.available)
        {
            root.voiceNoticePlace = null
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

        readingList.playReadAloud()
    }

    function readFromPlace(place)
    {
        root.closeLookup()

        if (!root.reader.readAloud.available)
        {
            root.voiceNoticePlace = place
            root.voiceNoticeShown = true

            return
        }

        root.voiceNoticeShown = false

        if (root.reader.isPdf)
        {
            pdfView.readFrom(place)

            return
        }

        readingList.readFrom(place)
    }

    function closeLookup()
    {
        translationPopup.dismiss()
        lookupSheet.close()
    }

    onPopupLookupChanged: if (!root.popupLookup) translationPopup.dismiss()

    function dismissSheet()
    {
        if (pageNumber.editing)
        {
            pageNumber.cancelEdit()

            return true
        }

        if (levelPicker.visible)
        {
            levelPicker.close()

            return true
        }

        if (confirmSheet.visible)
        {
            confirmSheet.close()

            return true
        }

        if (nameSheet.visible)
        {
            nameSheet.close()

            return true
        }

        if (filterDrawer.visible)
        {
            filterDrawer.close()

            return true
        }

        if (sortDrawer.visible)
        {
            sortDrawer.close()

            return true
        }

        if (sectionsSheet.visible)
        {
            sectionsSheet.close()

            return true
        }

        if (root.lookupOpen)
        {
            root.closeLookup()

            return true
        }

        if (root.outlineOpen || root.settingsOpen)
        {
            root.outlineOpen = false
            root.settingsOpen = false

            return true
        }

        if (bookPage.visible && !root.reading)
        {
            bookPage.close()

            return true
        }

        if (root.searchOpen && !root.reading)
        {
            root.searchOpen = false
            libraryShell.searchFieldText = ""
            root.forceActiveFocus()

            return true
        }

        return false
    }

    function openOutline()
    {
        root.outlineOpen = true

        if (root.reader.isPdf)
        {
            pdfView.requestOutline()
        }
    }

    function refresh()
    {
        if (!root.reader.reading)
        {
            root.reader.refresh()

            if (root.books.length === 0 && root.freeAvailable)
            {
                root.section = "free"
            }
        }
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
            root.reader.savePosition(root.topParagraph)
        }
    }

    function restorePosition()
    {
        if (!root.reader.reading || root.reader.loading || root.reader.isPdf)
        {
            return
        }

        Qt.callLater(function()
        {
            if (root.reader.currentPosition <= 0)
            {
                readingList.positionViewAtBeginning()

                return
            }

            readingList.positionViewAtIndex(root.reader.currentPosition, ListView.Beginning)
        })
    }

    function closeBook()
    {
        root.savePosition()

        root.closeLookup()
        root.outlineOpen = false
        root.settingsOpen = false

        root.reader.closeDocument()
        root.reader.refresh()
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

    Connections
    {
        target: root.reader

        function onFreeBookFailed(message) { toast.show(message) }
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
        target: root.reader.readAloud

        function onAvailableChanged()
        {
            if (!root.voiceNoticeShown || !root.reader.readAloud.available)
            {
                return
            }

            const place = root.voiceNoticePlace
            if (place !== null)
            {
                root.readFromPlace(place)

                return
            }

            root.voiceNoticeShown = false
            root.playReadAloud()
        }
    }

    Connections
    {
        target: root.reader

        function onCurrentDocumentChanged()
        {
            root.voiceNoticeShown = false
            root.pdfPendingPage = root.reader.currentPosition
            root.pdfPendingZoom = root.reader.currentZoom
            root.pdfPendingScroll = root.reader.currentScroll
        }

        function onVolumePageTurn(forward)
        {
            root.turnPage(forward)
        }

        function onLoadingChanged()
        {
            root.restorePosition()
        }
    }

    Component.onCompleted:
    {
        root.adoptShelf()
        root.refresh()
        root.restorePosition()
    }

    MobileShell
    {
        id: libraryShell

        anchors.fill: parent

        activeTab: "reader"
        headerTitle: root.reading ? root.reader.currentTitle : bookPage.visible ? "" : root.sectionTitle
        showBack: root.reading || root.searchOpen || bookPage.visible
        leadingIcon: "menu"
        showTabBar: !root.reading
        showHeader: !root.reading
        showSearchField: root.searchOpen

        onLeadingActivated: sectionsSheet.open()
        onSearchFieldAccepted: root.forceActiveFocus()

        headerStatus: RowLayout
        {
            Layout.fillWidth: false
            spacing: 0
            visible: !root.reading

            MobileIconButton
            {
                objectName: "reader_search"

                visible: !root.searchOpen && !bookPage.visible
                iconName: "search"
                text: qsTr("Search books")

                onClicked:
                {
                    root.searchOpen = true
                    Qt.callLater(libraryShell.focusSearchField)
                }
            }

            MobileIconButton
            {
                objectName: "reader_filter"

                visible: !root.searchOpen && !bookPage.visible
                iconName: "filter"
                marked: Shelf.filterCount(root.filter) > 0
                text: qsTr("Filter books")

                onClicked: filterDrawer.open()
            }

            MobileIconButton
            {
                objectName: "reader_sort"

                visible: !root.searchOpen && !bookPage.visible
                iconName: root.sortReversed ? "arrow_up_narrow_wide" : "arrow_down_wide_narrow"
                text: qsTr("Sort books")

                onClicked: sortDrawer.open()
            }

            MobileIconButton
            {
                objectName: "reader_layout_switch"

                visible: !root.searchOpen && !bookPage.visible
                iconName: root.listLayout ? "layout_grid" : "layout_rows"
                text: root.listLayout ? qsTr("Show as covers") : qsTr("Show as a list")

                onClicked: AppViewLayouts.setList("reader", !root.listLayout)
            }

            MobileIconButton
            {
                objectName: "book_page_rename"

                visible: bookPage.visible
                iconName: "edit"
                text: qsTr("Rename")

                onClicked: root.renameBook()
            }

            MobileIconButton
            {
                objectName: "book_page_favorite"

                readonly property bool on: bookPage.book !== null && bookPage.book.favorite === true

                visible: bookPage.visible && bookPage.book !== null
                iconName: "heart"
                filled: on
                iconColour: on ? AppTheme.colourWrong : AppTheme.colourText
                text: on ? qsTr("Remove from favorites") : qsTr("Add to favorites")

                onClicked: root.reader.setFavorite(bookPage.book.key, !on)
            }

            MobileIconButton
            {
                objectName: "book_page_open_with_header"

                visible: bookPage.visible && bookPage.canOpenWith && bookPage.openWithInHeader
                iconName: "external_link"
                text: bookPage.openWithText

                onClicked: bookPage.openWith()
            }
        }

        onBackRequested:
        {
            if (!root.dismissSheet())
            {
                root.closeBook()
            }
        }
        onTabSelected: function(key) { root.tabSelected(key) }

        Item
        {
            id: library

            anchors.fill: parent
            visible: !root.reading && !bookPage.visible

            Flickable
            {
                id: libraryPage

                anchors.fill: parent
                contentWidth: width
                contentHeight: libraryColumn.implicitHeight + libraryColumn.y * 2
                boundsBehavior: Flickable.StopAtBounds
                clip: true

                ColumnLayout
                {
                    id: libraryColumn

                    x: root.listLayout ? 0 : AppMetrics.contentPadding
                    y: root.listLayout ? 0 : AppMetrics.contentPadding
                    width: libraryPage.width - x * 2
                    spacing: AppMetrics.gapMedium

                    Repeater
                    {
                        model: root.shownGroups

                        delegate: ColumnLayout
                        {
                            id: shelfGroup

                            required property var modelData
                            required property int index

                            readonly property int first: root.shownGroups.slice(0, shelfGroup.index)
                                                             .reduce(function(total, group) { return total + group.books.length }, 0)

                            Layout.fillWidth: true
                            spacing: AppMetrics.gapSmall

                            Text
                            {
                                Layout.fillWidth: true
                                Layout.leftMargin: root.listLayout ? AppMetrics.contentPadding : 0
                                Layout.topMargin: root.listLayout ? AppMetrics.gapMedium : 0
                                visible: shelfGroup.modelData.title.length > 0
                                text: shelfGroup.modelData.title
                                font.pixelSize: AppMetrics.fontMicro
                                font.weight: Font.DemiBold
                                font.family: AppMetrics.fontFamily
                                font.letterSpacing: AppMetrics.trackingWide
                                font.capitalization: Font.AllUppercase
                                color: AppTheme.colourMuted
                                elide: Text.ElideRight
                            }

                            MobileCoverGrid
                            {
                                id: coverGrid

                                listLayout: root.listLayout

                                Accessible.role: Accessible.List
                                Accessible.name: root.sectionTitle

                                Repeater
                                {
                                    model: shelfGroup.modelData.books

                                    delegate: MobileReaderBook
                                    {
                                        id: bookTile

                                        required property var modelData
                                        required property int index

                                        objectName: "reader_book_" + (shelfGroup.first + bookTile.index)

                                        width: coverGrid.tileWidth
                                        book: modelData
                                        compact: root.listLayout
                                        cloudAvailable: root.reader.cloudAvailable
                                        progress:
                                        {
                                            const value = !modelData.local && modelData.freeId !== undefined
                                                        ? root.reader.freeTransfers[modelData.freeId]
                                                        : root.reader.transfers[modelData.local ? modelData.id : modelData.cloudId]

                                            return value === undefined ? -1 : value
                                        }

                                        onClicked: root.openShelfBook(modelData, bookTile.transferring)
                                        onDownloadRequested: root.openShelfBook(modelData, false)
                                        onDetailsRequested: bookPage.show(modelData.id)
                                        onFavoriteToggled: root.reader.setFavorite(modelData.key, !bookTile.favorite)
                                        onCollectionsRequested: root.showCollections(modelData)
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

                    Text
                    {
                        Layout.fillWidth: true
                        Layout.margins: root.listLayout ? AppMetrics.contentPadding : 0
                        visible: root.shelf.length === 0

                        text: root.searchTerm.length > 0 ? qsTr("No books match \"%1\"").arg(root.searchTerm)
                            : Shelf.filterCount(root.filter) > 0 ? qsTr("No books match the filter.")
                            : qsTr("No books here yet.")
                        color: AppTheme.colourMuted
                        font.pixelSize: AppMetrics.fontBody
                        font.family: AppMetrics.fontFamily
                        wrapMode: Text.WordWrap
                    }

                    MobileWideButton
                    {
                        objectName: "reader_import_empty"

                        Layout.leftMargin: root.listLayout ? AppMetrics.contentPadding : 0
                        Layout.rightMargin: root.listLayout ? AppMetrics.contentPadding : 0
                        visible: root.section === "all" && root.books.length === 0
                        label: qsTr("Import a book")

                        onClicked: root.reader.pickAndOpen()
                    }
                }
            }
        }

        MobileReaderBookPage
        {
            id: bookPage

            anchors.fill: parent
            controller: root.reader
            openWithInHeader: root.height > root.width

            onReadRequested: function(book) { root.openBookAt(book) }
            onChapterRequested: function(book, position) { root.openBookAt(book, position) }
            onCollectionsRequested: function(book) { root.showCollections(book) }
        }

        Item
        {
            id: reading

            anchors.fill: parent
            visible: root.reading && !root.cannotRead

            MobileReaderFrame
            {
                id: readingFrame

                objectName: "reader_frame"
                anchors.fill: parent
                controlsVisible: root.chromeVisible

                topContent: [
                    MobileHeader
                    {
                        visible: root.chromeVisible
                        title: root.reader.currentTitle
                        showBack: true

                        onBackRequested: root.closeBook()

                        status: RowLayout
                        {
                            Layout.fillWidth: false
                            spacing: 0

                            MobileIconButton
                            {
                                objectName: "reader_read_aloud"

                                visible: root.readAloudOffered
                                iconName: "headphones"
                                iconColour: root.reader.readAloud.started || root.voiceNoticeShown ? AppTheme.colourAccent : AppTheme.colourText
                                text: qsTr("Read aloud")

                                onClicked: root.toggleReadAloud()
                            }

                            MobileIconButton
                            {
                                objectName: "reader_contents"

                                iconName: "contents"
                                iconColour: root.outlineOpen ? AppTheme.colourAccent : AppTheme.colourText
                                text: qsTr("Contents")

                                onClicked: root.openOutline()
                            }

                            MobileIconButton
                            {
                                objectName: "reader_appearance"

                                iconName: "text_size"
                                iconColour: root.settingsOpen ? AppTheme.colourAccent : AppTheme.colourText
                                text: qsTr("Appearance")

                                onClicked: root.settingsOpen = true
                            }

                            MobileIconButton
                            {
                                objectName: "reader_lookup"

                                iconName: "search"
                                iconColour: root.lookupOpen ? AppTheme.colourAccent : AppTheme.colourText
                                text: qsTr("Look up a word")

                                onClicked: root.openLookup("", true)
                            }
                        }
                    }
                ]

                bottomContent: [
                    Rectangle
                    {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 43
                        visible: root.reader.isPdf

                        radius: AppMetrics.radiusMedium
                        color: AppTheme.colourPanel
                        border.width: AppMetrics.borderWidth
                        border.color: AppTheme.colourBorder

                        MobileIconButton
                        {
                            anchors.left: parent.left
                            anchors.leftMargin: AppMetrics.gapSmall
                            anchors.verticalCenter: parent.verticalCenter
                            visible: root.reader.isPdf
                            enabled: pdfView.pageIndex > 0

                            iconName: "chevron_left"
                            iconColour: enabled ? AppTheme.colourText : AppTheme.colourSubtle
                            text: qsTr("Previous page")

                            onClicked: pdfView.goToPage(pdfView.pageIndex - 1)
                        }

                        MobileReaderPageNumber
                        {
                            id: pageNumber

                            anchors.centerIn: parent
                            width: Math.min(180, parent.width - 112)
                            height: parent.height
                            page: pdfView.pageIndex
                            pageCount: pdfView.pageCount
                            editable: root.reader.isPdf
                            text: root.reader.isPdf
                                ? (pdfView.pageIndex + 1) + " / " + Math.max(pdfView.pageCount, 1)
                                : qsTr("%1%").arg(root.percentOf(root.topParagraph))

                            onPageRequested: function(page) { pdfView.goToPage(page) }
                        }

                        MobileIconButton
                        {
                            anchors.right: parent.right
                            anchors.rightMargin: AppMetrics.gapSmall
                            anchors.verticalCenter: parent.verticalCenter
                            visible: root.reader.isPdf
                            enabled: pdfView.pageIndex + 1 < pdfView.pageCount

                            iconName: "chevron_right"
                            iconColour: enabled ? AppTheme.colourText : AppTheme.colourSubtle
                            text: qsTr("Next page")

                            onClicked: pdfView.goToPage(pdfView.pageIndex + 1)
                        }
                    },

                    RowLayout
                    {
                        Layout.fillWidth: true
                        Layout.topMargin: AppMetrics.gapSmall
                        spacing: AppMetrics.gapMedium
                        visible: root.chromeVisible && root.positionCount > 1

                        Text
                        {
                            visible: !root.reader.isPdf
                            Layout.preferredWidth: percentMetrics.advanceWidth

                            text: qsTr("%1%").arg(root.percentOf(scrubber.shownIndex))
                            font.pixelSize: AppMetrics.fontMeta
                            font.family: AppMetrics.fontFamily
                            color: AppTheme.colourMuted
                            horizontalAlignment: Text.AlignLeft

                            TextMetrics
                            {
                                id: percentMetrics

                                font.pixelSize: AppMetrics.fontMeta
                                font.family: AppMetrics.fontFamily
                                text: qsTr("100%")
                            }
                        }

                        ReaderScrubber
                        {
                            id: scrubber

                            Layout.fillWidth: true

                            count: root.positionCount
                            index: root.positionIndex

                            onPositionRequested: function(position) { root.goToPosition(position) }

                            Rectangle
                            {
                                visible: scrubber.pressed
                                z: 1

                                x: Math.max(0, Math.min(scrubber.width - width, scrubber.handle.x + scrubber.handle.width / 2 - width / 2))
                                y: scrubber.handle.y - height - AppMetrics.gapSmall

                                width: bubbleLabel.implicitWidth + AppMetrics.gapMedium
                                height: bubbleLabel.implicitHeight + AppMetrics.gapSmall
                                radius: AppMetrics.radiusMedium
                                color: AppTheme.colourPanel
                                border.width: AppMetrics.borderWidth
                                border.color: AppTheme.colourBorder

                                Text
                                {
                                    id: bubbleLabel
                                    anchors.centerIn: parent
                                    text: root.reader.isPdf
                                        ? scrubber.shownIndex + 1
                                        : qsTr("%1%").arg(root.percentOf(scrubber.shownIndex))
                                    font.pixelSize: AppMetrics.fontMeta
                                    font.family: AppMetrics.fontFamily
                                    color: AppTheme.colourText
                                }
                            }
                        }
                    }
                ]

                Item
                {
                    id: pageArea

                    anchors.fill: parent

                    MobileReaderPdfView
                    {
                        id: pdfView

                        anchors.fill: parent
                        visible: root.reader.isPdf

                        source: root.reader.isPdf ? root.reader.currentPath : ""
                        initialPage: root.reader.currentPosition
                        initialZoom: root.reader.currentZoom
                        initialScroll: root.reader.currentScroll
                        cropMargins: root.reader.cropMargins
                        startPadding: readingFrame.topReserve + AppMetrics.readerEndSpace
                        endPadding: readingFrame.bottomReserve + AppMetrics.readerEndSpace
                        paperColour: root.pageTone.paper
                        inkColour: root.pageTone.ink
                        readAloud: root.reader.readAloud
                        narration: root.reader.pdfNarration
                        topChrome: readingFrame.topCover
                        readAloudInset: readingFrame.bottomCover + (playBar.visible ? playBar.height + AppMetrics.gapMedium * 2 : 0)

                        onWordPicked: function(word, sentence)
                        {
                            if (root.reader.readAloud.playing)
                            {
                                root.reader.readAloud.pause()
                            }

                            root.openLookup(word, false, sentence, pdfView.pickedPlace)
                        }
                        onDoubleTapped: root.closeLookup()
                        onReadAloudRequested: function(place) { root.readFromPlace(place) }
                        onBlankTapped: root.toggleChrome()
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

                    Rectangle
                    {
                        anchors.fill: readingList
                        visible: !root.reader.isPdf && root.textToned
                        color: root.textToned ? root.textTone.paper : "transparent"
                    }

                    ListView
                    {
                        id: readingList

                        anchors.fill: parent
                        spacing: readingList.styled ? 0 : Math.round(root.reader.textSize * 0.6)
                        clip: true
                        visible: !root.reader.isPdf

                        readonly property bool styled: root.reader.paragraphModel !== null
                            && root.reader.paragraphModel !== undefined && root.reader.paragraphModel.styled === true

                        model: root.reader.paragraphModel

                        header: Item
                        {
                            width: readingList.width
                            height: readingFrame.topReserve + AppMetrics.readerEndSpace
                        }

                        footer: Item
                        {
                            width: readingList.width
                            height: readingFrame.bottomReserve + AppMetrics.readerEndSpace
                        }

                        property var selectedText: null
                        property bool followingReadAloud: true
                        property bool readingAway: false
                        readonly property real readAloudTop: readingFrame.topCover
                        readonly property real readAloudBottom: readingFrame.bottomCover
                            + (playBar.visible ? playBar.height + AppMetrics.gapMedium * 2 : 0)

                        function leaveReadAloud()
                        {
                            readingList.followingReadAloud = false
                            followGlide.stop()
                        }

                        function playReadAloud()
                        {
                            readingList.followingReadAloud = true
                            ReadAloudPlace.play(readingList, root.reader.readAloud, root.topParagraph,
                                                readingList.contentY + readingList.readAloudTop,
                                                readingList.contentY + readingList.height - readingList.readAloudBottom)
                        }

                        function readFrom(place)
                        {
                            if (place === null)
                            {
                                return
                            }

                            readingList.clearSelection()
                            readingList.followingReadAloud = true
                            root.reader.readAloud.startAt(place.paragraph, place.character)
                        }

                        function showReading()
                        {
                            readingList.followingReadAloud = true
                            readingList.followReadAloud(true)
                        }

                        function updateReadingAway()
                        {
                            readingList.readingAway = root.reader.readAloud.started && !readingList.followingReadAloud
                                && !ReadAloudPlace.isReadingShown(readingList, root.reader.readAloud,
                                                                  readingList.contentY + readingList.readAloudTop,
                                                                  readingList.contentY + readingList.height - readingList.readAloudBottom)
                        }

                        function followReadAloud(force)
                        {
                            if (!root.reader.readAloud.started || !readingList.followingReadAloud
                                || (!force && !root.reader.readAloud.playing))
                            {
                                return
                            }

                            let span = ReadAloudPlace.sentenceSpan(readingList, root.reader.readAloud)

                            if (span === null)
                            {
                                followGlide.stop()
                                readingList.positionViewAtIndex(root.reader.readAloud.paragraph, ListView.Beginning)
                                readingList.forceLayout()
                                span = ReadAloudPlace.sentenceSpan(readingList, root.reader.readAloud)

                                if (span === null)
                                {
                                    return
                                }
                            }

                            const visibleTop = readingList.contentY + readingList.readAloudTop
                            const visibleBottom = readingList.contentY + readingList.height - readingList.readAloudBottom

                            if (span.top >= visibleTop && span.bottom <= visibleBottom)
                            {
                                return
                            }

                            const first = readingList.originY
                            const last = Math.max(first, readingList.originY + readingList.contentHeight - readingList.height)

                            followGlide.stop()
                            followGlide.from = readingList.contentY
                            followGlide.to = Math.max(first, Math.min(last, span.top - readingList.readAloudTop - root.reader.textSize * 2))
                            followGlide.start()
                        }

                        onMovementStarted:
                        {
                            readingList.leaveReadAloud()
                            translationPopup.dismiss()
                        }

                        function selectionGeometry()
                        {
                            const owner = readingList.selectedText

                            if (owner === null || owner.selectedText.length === 0)
                            {
                                return null
                            }

                            const first = owner.positionToRectangle(owner.selectionStart)
                            const last = owner.positionToRectangle(owner.selectionEnd)
                            const left = owner.parent.x + owner.x
                            const top = owner.parent.y + owner.y - readingList.contentY
                            const singleLine = first.y === last.y
                            const selectionLeft = left + (singleLine ? Math.min(first.x, last.x) : 0)
                            const selectionRight = left + (singleLine ? Math.max(first.x, last.x) : owner.width)

                            return { top: top + first.y, bottom: top + last.y + last.height, endX: left + last.x,
                                     endY: top + last.y + last.height / 2, left: selectionLeft, right: selectionRight,
                                     width: selectionRight - selectionLeft }
                        }

                        NumberAnimation
                        {
                            id: followGlide

                            target: readingList
                            property: "contentY"
                            duration: AppMetrics.motionSlow
                            easing.type: Easing.InOutCubic
                        }

                        Connections
                        {
                            target: root.reader.readAloud

                            function onStateChanged()
                            {
                                Qt.callLater(readingList.followReadAloud, false)
                                Qt.callLater(readingList.updateReadingAway)
                            }
                        }

                        onContentYChanged:
                        {
                            positionSaveTimer.restart()
                            readingList.updateReadingAway()
                        }
                        onHeightChanged: readingList.updateReadingAway()
                        onFollowingReadAloudChanged: readingList.updateReadingAway()

                        function claimSelection(owner)
                        {
                            if (readingList.selectedText !== null && readingList.selectedText !== owner)
                            {
                                readingList.selectedText.deselect()
                            }

                            readingList.selectedText = owner
                        }

                        function clearSelection()
                        {
                            translationPopup.dismiss()

                            if (readingList.selectedText === null)
                            {
                                return
                            }

                            readingList.selectedText.deselect()
                            readingList.selectedText = null
                        }

                        TapHandler
                        {
                            onSingleTapped: function(eventPoint)
                            {
                                const row = readingList.itemAt(eventPoint.position.x, eventPoint.position.y)

                                if (row === null || !row.holdsText(eventPoint.position.x))
                                {
                                    root.toggleChrome()
                                    readingList.clearSelection()
                                }
                            }
                        }

                        property var selectingText: null
                        property int selectionAnchorStart: 0
                        property int selectionAnchorEnd: 0

                        delegate: Item
                        {
                            id: paragraph

                            required property string display
                            required property int heading
                            required property int index
                            required property string image
                            required property int imageWidth
                            required property int imageHeight
                            required property bool rule
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
                            required property string textAlign
                            required property real fontScale
                            required property bool bold
                            required property bool italic
                            required property bool smallCaps
                            required property string textTransform

                            readonly property bool pictured: paragraph.image.length > 0
                            readonly property real textWidth: paragraph.width - root.reader.textMargin * 2
                            readonly property real pictureScale: paragraph.imageWidth > 0 && paragraph.imageHeight > 0
                                ? Math.min(1, paragraph.textWidth / paragraph.imageWidth,
                                           readingList.height * AppMetrics.readerImageHeightShare / paragraph.imageHeight)
                                : 1
                            readonly property real topGap:
                            {
                                if (paragraph.index === 0)
                                {
                                    return 0
                                }

                                if (readingList.styled)
                                {
                                    return Math.max(
                                        paragraph.previousMarginBottom * root.reader.textSize
                                            + paragraph.previousMarginBottomWidth * paragraph.textWidth,
                                        paragraph.marginTop * root.reader.textSize
                                            + paragraph.marginTopWidth * paragraph.textWidth)
                                }

                                return paragraph.heading > 0 ? root.reader.textSize : 0
                            }

                            width: ListView.view.width
                            height: (paragraph.pictured ? picture.height
                                                        : (paragraph.rule ? root.reader.textSize : paragraphText.implicitHeight))
                                    + paragraph.topGap

                            function sentenceBounds(start, end)
                            {
                                const first = paragraphText.positionToRectangle(start)
                                const last = paragraphText.positionToRectangle(Math.max(start, end - 1))

                                return { top: paragraphText.y + first.y, bottom: paragraphText.y + last.y + last.height }
                            }

                            function characterAt(y)
                            {
                                return paragraphText.positionAt(0, Math.max(0, y - paragraphText.y))
                            }

                            function holdsText(across)
                            {
                                return !paragraph.pictured && !paragraph.rule
                                    && across >= paragraphText.x && across < paragraphText.x + paragraphText.width
                            }

                            Rectangle
                            {
                                visible: paragraph.rule
                                x: root.reader.textMargin + (paragraph.textWidth - width) / 2
                                y: paragraph.topGap + (root.reader.textSize - height) / 2
                                width: Math.round(paragraph.textWidth / 3)
                                height: AppMetrics.borderWidth
                                color: root.textInk
                                opacity: 0.4
                            }

                            Image
                            {
                                id: picture

                                visible: paragraph.pictured
                                x: root.reader.textMargin + (paragraph.textWidth - width) / 2
                                y: paragraph.topGap
                                width: paragraph.imageWidth > 0
                                    ? Math.round(paragraph.imageWidth * paragraph.pictureScale)
                                    : Math.min(paragraph.textWidth, implicitWidth)
                                height: paragraph.imageHeight > 0
                                    ? Math.round(paragraph.imageHeight * paragraph.pictureScale)
                                    : (implicitWidth > 0 ? width * implicitHeight / implicitWidth : 0)
                                source: paragraph.pictured
                                    ? "image://book/" + encodeURIComponent(root.reader.currentPath) + "/"
                                      + encodeURIComponent(paragraph.image)
                                    : ""
                                sourceSize: Qt.size(Math.round(width * Screen.devicePixelRatio),
                                                    Math.round(height * Screen.devicePixelRatio))
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
                                x: root.reader.textMargin
                                y: paragraph.topGap
                                width: parent.width - root.reader.textMargin * 2

                                text: "<p" + (paragraph.textAlign.length > 0 ? " align='" + paragraph.textAlign + "'" : "")
                                    + " style='margin:0; line-height:" + root.reader.lineSpacing + "%"
                                    + "; margin-left:" + Math.round(paragraph.marginLeft * root.reader.textSize
                                                                    + paragraph.marginLeftWidth * paragraph.textWidth) + "px"
                                    + "; margin-right:" + Math.round(paragraph.marginRight * root.reader.textSize
                                                                     + paragraph.marginRightWidth * paragraph.textWidth) + "px"
                                    + (paragraph.textIndent !== 0 || paragraph.textIndentWidth !== 0
                                       ? "; text-indent:" + Math.round(paragraph.textIndent * root.reader.textSize
                                                                       + paragraph.textIndentWidth * paragraph.textWidth) + "px"
                                       : "")
                                    + "'>"
                                    + (paragraph.markup.length > 0 ? paragraph.markup : root.escapeHtml(paragraph.display))
                                    + "</p>"
                                textFormat: TextEdit.RichText
                                color: root.textInk
                                font.pixelSize: Math.round(root.reader.textSize * (readingList.styled
                                    ? paragraph.fontScale
                                    : (paragraph.heading > 0 ? root.headingScale(paragraph.heading) : 1)))
                                font.weight: readingList.styled ? (paragraph.bold ? Font.Bold : Font.Normal)
                                                                : (paragraph.heading > 0 ? Font.DemiBold : Font.Normal)
                                font.italic: paragraph.italic
                                font.capitalization: paragraph.smallCaps ? Font.SmallCaps
                                                                         : root.capitalization(paragraph.textTransform)
                                font.family: root.textFont

                                wrapMode: TextEdit.Wrap
                                readOnly: true
                                activeFocusOnPress: false
                                selectByMouse: false
                                persistentSelection: true
                                cursorVisible: false
                                selectionColor: AppTheme.colourAccent
                                selectedTextColor: AppTheme.colourAccentText

                                ReaderTextHitTest
                                {
                                    id: hitTest

                                    textDocument: paragraphText.textDocument
                                }

                                ReaderTextHighlighter
                                {
                                    readonly property bool reading: root.reader.readAloud.started
                                                                    && root.reader.readAloud.paragraph === paragraph.index

                                    textDocument: root.reader.vocabulary.enabled || reading ? paragraphText.textDocument : null
                                    vocabulary: root.reader.vocabulary
                                    darkPaper: root.textInk.hslLightness > 0.5
                                    learningColour: darkPaper ? AppTheme.colourReaderMarkDark : AppTheme.colourAccent
                                    sentenceStart: reading ? root.reader.readAloud.sentenceStart : -1
                                    sentenceEnd: reading ? root.reader.readAloud.sentenceEnd : -1
                                    sentenceColour: AppTheme.faded(root.textInk, AppTheme.alphaSoft)
                                }

                                TapHandler
                                {
                                    id: wordTapHandler

                                    exclusiveSignals: TapHandler.SingleTap | TapHandler.DoubleTap
                                    longPressThreshold: 0.4

                                    function wordRangeAt(position)
                                    {
                                        return root.wordRange(paragraph.display, hitTest.characterAt(position.x, position.y))
                                    }

                                    onSingleTapped: function(eventPoint)
                                    {
                                        const range = wordTapHandler.wordRangeAt(paragraphText.mapFromItem(null, eventPoint.scenePosition))

                                        if (range === null)
                                        {
                                            root.toggleChrome()
                                            readingList.clearSelection()

                                            return
                                        }

                                        root.reader.readAloud.pause()
                                        readingList.claimSelection(paragraphText)
                                        paragraphText.select(range.start, range.end)
                                        root.openLookup(paragraph.display.substring(range.start, range.end),
                                                        false, lookupSheet.dictionary.sentenceAt(paragraph.display, range.start),
                                                        { paragraph: paragraph.index, character: range.start })
                                    }

                                    onDoubleTapped: function(eventPoint)
                                    {
                                        const range = wordTapHandler.wordRangeAt(paragraphText.mapFromItem(null, eventPoint.scenePosition))

                                        if (range !== null)
                                        {
                                            root.readFromPlace({ paragraph: paragraph.index, character: range.start })
                                        }
                                    }

                                    onLongPressed:
                                    {
                                        const range = wordTapHandler.wordRangeAt(selectionTracker.point.position)

                                        if (range === null)
                                        {
                                            return
                                        }

                                        root.reader.readAloud.pause()
                                        readingList.interactive = false
                                        readingList.claimSelection(paragraphText)
                                        readingList.selectingText = paragraphText
                                        readingList.selectionAnchorStart = range.start
                                        readingList.selectionAnchorEnd = range.end
                                        paragraphText.select(range.start, range.end)
                                    }
                                }

                                PointHandler
                                {
                                    id: selectionTracker

                                    readonly property point livePoint: selectionTracker.point.position

                                    onLivePointChanged:
                                    {
                                        if (readingList.selectingText !== paragraphText)
                                        {
                                            return
                                        }

                                        const index = hitTest.positionNear(
                                            selectionTracker.livePoint.x, selectionTracker.livePoint.y)
                                        const range = root.wordRange(paragraph.display, index)
                                            || { start: index, end: index }

                                        paragraphText.select(
                                            Math.min(readingList.selectionAnchorStart, range.start),
                                            Math.max(readingList.selectionAnchorEnd, range.end))
                                    }

                                    onActiveChanged:
                                    {
                                        if (selectionTracker.active
                                            || readingList.selectingText !== paragraphText)
                                        {
                                            return
                                        }

                                        readingList.selectingText = null
                                        readingList.interactive = true

                                        const phrase = paragraph.display.substring(
                                            paragraphText.selectionStart, paragraphText.selectionEnd).trim()

                                        if (phrase.length > 0)
                                        {
                                            root.openLookup(phrase, false, lookupSheet.dictionary.sentenceAt(paragraph.display, paragraphText.selectionStart),
                                                            { paragraph: paragraph.index, character: paragraphText.selectionStart })
                                        }
                                    }
                                }
                            }
                        }
                    }

                    MobileReaderTranslation
                    {
                        id: translationPopup

                        anchors.fill: readingList
                        z: 40
                        visible: !root.reader.isPdf

                        contextLookup: root.contextDictionary
                        lookup: root.translationDictionary
                        maxStage: root.stageCount
                        geometry: readingList.selectionGeometry()
                        readAloudOffered: !root.reader.isPdf && root.reader.readAloud.available && root.lookupPlace !== null
                        knownOffered: true

                        onKnownMarkChanged: root.reader.vocabulary.refresh()

                        onAddWordRequested: function(word, translation, partOfSpeech, hint)
                        {
                            translationPopup.dismiss()
                            root.addWordRequested(word, translation, partOfSpeech, hint)
                        }

                        onDictionaryRequested: function(word)
                        {
                            translationPopup.dismiss()
                            root.entryOpened(word, "", translationPopup.sentence)
                        }

                        onReadAloudRequested: root.readFromPlace(root.lookupPlace)
                    }
                }
            }

            MobileReaderPlayBar
            {
                id: playBar

                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: readingFrame.bottomCover
                                      + (readingFrame.controlsVisible ? 0 : readingFrame.SafeArea.margins.bottom)
                visible: root.readAloudOffered && !root.reader.loading && root.reader.readAloud.started

                readAloud: root.reader.readAloud

                onPlayRequested: root.playReadAloud()
            }

            Rectangle
            {
                objectName: "reader_read_aloud_back"

                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: playBar.top
                anchors.bottomMargin: AppMetrics.gapSmall
                visible: playBar.visible && (root.reader.isPdf ? pdfView.readingAway : readingList.readingAway)
                implicitWidth: backLabel.implicitWidth + AppMetrics.gapMedium * 2
                implicitHeight: backLabel.implicitHeight + AppMetrics.gapSmall * 2
                radius: height / 2
                color: AppTheme.colourCard
                border.width: AppMetrics.borderWidth
                border.color: AppTheme.colourBorder

                Text
                {
                    id: backLabel

                    anchors.centerIn: parent
                    text: qsTr("Back to reading")
                    color: AppTheme.colourText
                    font.pixelSize: AppMetrics.fontBody
                    font.weight: Font.Medium
                    font.family: AppMetrics.fontFamily
                }

                MouseArea
                {
                    anchors.fill: parent

                    onClicked: root.reader.isPdf ? pdfView.showReading() : readingList.showReading()
                }
            }

            MobileReaderVoiceNotice
            {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: playBar.anchors.bottomMargin
                visible: root.voiceNoticeShown && root.readAloudOffered && !root.reader.loading && !root.reader.readAloud.available

                readAloud: root.reader.readAloud

                onClosed: root.voiceNoticeShown = false
            }

            LoadingScreen
            {
                anchors.fill: parent
                visible: root.reader.loading || (root.reader.isPdf && !pdfView.presented)
                message: qsTr("Opening book")
            }

            Timer
            {
                id: positionSaveTimer
                interval: 800

                onTriggered: root.savePosition()
            }
        }

        Item
        {
            anchors.fill: parent
            visible: root.cannotRead

            ColumnLayout
            {
                anchors.centerIn: parent
                width: Math.min(parent.width - 48, 460)
                spacing: AppMetrics.gapMedium

                Text
                {
                    Layout.fillWidth: true
                    text: qsTr("Cannot read this book")
                    color: AppTheme.colourText
                    font.pixelSize: AppMetrics.fontLabel
                    font.family: AppMetrics.fontFamily
                    horizontalAlignment: Text.AlignHCenter
                }

                Text
                {
                    Layout.fillWidth: true
                    text: qsTr("It may be corrupt, or protected in a way this reader cannot open.")
                    color: AppTheme.colourMuted
                    font.pixelSize: AppMetrics.fontBody
                    font.family: AppMetrics.fontFamily
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }

                MobileWideButton
                {
                    label: qsTr("Back to library")
                    onClicked: root.closeBook()
                }

                MobileWideButton
                {
                    label: qsTr("Remove from library")
                    filled: false
                    onClicked:
                    {
                        const id = root.reader.currentId
                        root.closeBook()
                        root.reader.removeDocument(id)
                    }
                }
            }
        }
    }

    MobileSettingsPicker
    {
        id: levelPicker

        anchors.fill: parent
        z: 60
    }

    MobileLookupSheet
    {
        id: lookupSheet

        lookup: root.lookupDictionary
        stageCount: root.stageCount

        anchors.fill: parent
        readerMode: true

        onAddWordRequested: function(word, translation, partOfSpeech, hint)
        {
            root.addWordRequested(word, translation, partOfSpeech, hint)
        }

        onEntryOpened: function(headword, partOfSpeech, hint) { root.entryOpened(headword, partOfSpeech, hint) }
    }

    MobileBottomSheet
    {
        anchors.fill: parent
        z: 50
        visible: root.outlineOpen

        heading: qsTr("Contents")

        onDismissed: root.outlineOpen = false

        Text
        {
            Layout.fillWidth: true
            visible: root.contentsLoading || root.contentsEntries.length === 0

            text: root.contentsLoading ? qsTr("Loading…") : qsTr("No table of contents in this book")
            color: AppTheme.colourMuted
            font.pixelSize: AppMetrics.fontBody
            font.family: AppMetrics.fontFamily
            wrapMode: Text.WordWrap
        }

        ListView
        {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(contentHeight, root.height * 0.6)
            visible: !root.contentsLoading && root.contentsEntries.length > 0

            clip: true
            model: root.contentsEntries

            delegate: Item
            {
                required property var modelData

                width: ListView.view.width
                height: entryLabel.implicitHeight + AppMetrics.gapMedium * 2

                Text
                {
                    id: entryLabel
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.right: entryPage.left
                    anchors.leftMargin: parent.modelData.level * AppMetrics.gapMedium
                    anchors.rightMargin: AppMetrics.gapSmall
                    text: parent.modelData.title
                    elide: Text.ElideRight
                    font.pixelSize: AppMetrics.fontBody
                    font.family: AppMetrics.fontFamily
                    color: AppTheme.colourText
                }

                Text
                {
                    id: entryPage
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: parent.right
                    text: root.reader.isPdf
                        ? parent.modelData.page + 1
                        : qsTr("%1%").arg(root.percentOf(parent.modelData.page))
                    font.pixelSize: AppMetrics.fontMeta
                    font.family: AppMetrics.fontFamily
                    color: AppTheme.colourMuted
                }

                MouseArea
                {
                    anchors.fill: parent
                    onClicked:
                    {
                        root.goToPosition(parent.modelData.page)
                        root.outlineOpen = false
                    }
                }
            }
        }
    }

    MobileBottomSheet
    {
        anchors.fill: parent
        z: 50
        visible: root.settingsOpen

        heading: qsTr("Reading")

        onDismissed: root.settingsOpen = false

        overlays: MobileSettingsPicker
        {
            id: tonePicker
        }

        MobileSettingsReader
        {
            optionPicker: tonePicker
            showPdf: root.reader.isPdf
            showText: !root.reader.isPdf
            titled: false
            readAloud: root.reader.readAloud
        }
    }

    MobileReaderSectionsSheet
    {
        id: sectionsSheet

        anchors.fill: parent
        z: 40

        section: root.section
        counts: Shelf.sectionCounts(root.books, root.reader.freeBooks, root.reader.collections)
        collections: root.reader.collections
        freeShown: root.freeAvailable

        onSectionChosen: function(section) { root.chooseSection(section) }
        onNewCollectionRequested: root.createCollection("")
        onCollectionHeld: function(collectionId, name)
        {
            sectionsSheet.close()
            root.showCollectionActions(collectionId, name)
        }
        onImportRequested: root.reader.pickAndOpen()
    }

    MobileReaderFilterDrawer
    {
        id: filterDrawer

        anchors.fill: parent
        z: 50

        filter: root.filter
        formats: Shelf.formats(root.sourceBooks)
        resultCount: root.shelf.length

        onToggled: function(key, value) { root.filter = Shelf.toggledFilter(root.filter, key, value) }
        onReset: root.filter = Shelf.emptyFilter()
    }

    MobileReaderSortDrawer
    {
        id: sortDrawer

        anchors.fill: parent
        z: 50

        order: root.sortOrder
        reversed: root.sortReversed
        groupMode: root.groupMode
        directions: Shelf.libraryOrderDirections()

        onOrderChosen: function(order) { AppReaderSettings.librarySortOrder = order }
        onReversedChosen: function(reversed) { AppReaderSettings.librarySortReversed = reversed }
        onGroupChosen: function(group) { AppReaderSettings.libraryGroup = group }
    }

    MobileNameSheet
    {
        id: nameSheet

        z: 55
    }

    MobileConfirmSheet
    {
        id: confirmSheet
    }

    MobileToast
    {
        id: toast

        z: 60
    }
}
