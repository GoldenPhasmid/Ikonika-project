import QtQuick
import QtQuick.Layouts
import "MobileIcons.js" as Icons
import "../ReaderTones.js" as Tones
import "../ReaderShelf.js" as Shelf
import "../ReadAloudPlace.js" as ReadAloudPlace

Item
{
    id: root

    signal tabSelected(string key)
    signal addWordRequested(string word, string translation, string partOfSpeech, string hint)
    signal entryOpened(string headword, string partOfSpeech, string hint)

    readonly property bool reading: reader.reading

    readonly property bool lookupOpen: lookupSheet.shown
    property bool outlineOpen: false
    property bool settingsOpen: false
    property bool voiceNoticeShown: false
    property var lookupPlace: null

    property int pdfPendingPage: 0
    property real pdfPendingZoom: 0
    property point pdfPendingScroll: Qt.point(0, 0)

    property bool searchOpen: false
    property string sortOrder: "recent"
    property bool freeShown: false
    property string levelFilter: ""
    readonly property var books: reader.documents.filter(document => !document.local || document.exists)
    readonly property bool freeAvailable: reader.freeBooks.length > 0
    readonly property string searchTerm: (libraryShell.searchFieldText + libraryShell.searchFieldPreeditText).trim()
    readonly property var shelf: root.freeShown ? Shelf.arrange(reader.freeBooks, root.sortOrder, root.searchTerm, false, root.levelFilter)
                                                : Shelf.arrange(root.books, root.sortOrder, root.searchTerm)

    onFreeAvailableChanged:
    {
        if (!root.freeAvailable)
        {
            root.freeShown = false
        }
        else if (root.books.length === 0)
        {
            root.freeShown = true
        }
    }
    readonly property bool listLayout: AppViewLayouts.revision >= 0 && AppViewLayouts.isList("reader")

    readonly property int topParagraph: Math.max(0, readingList.indexAt(1, readingList.contentY + 1))
    readonly property bool cannotRead: root.reading
        && (reader.unreadable || (reader.isPdf && pdfView.failed))
    readonly property bool chromeVisible: !reader.fullScreen

    readonly property int positionCount: reader.isPdf ? pdfView.pageCount : readingList.count
    readonly property int positionIndex: reader.isPdf ? pdfView.pageIndex : root.topParagraph
    readonly property int loadedPositionCount: root.reading && !root.cannotRead && !reader.loading
        && (!reader.isPdf || pdfView.ready) ? root.positionCount : 0

    onLoadedPositionCountChanged:
    {
        if (root.loadedPositionCount > 0)
        {
            reader.savePositionCount(root.loadedPositionCount)
        }
    }

    readonly property var contentsEntries: reader.isPdf ? pdfView.outlineEntries : reader.chapters

    readonly property var pageTone: Tones.find(reader.pageTone)
    readonly property var textTone: Tones.findText(AppReaderSettings.textTone)
    readonly property bool textToned: root.textTone.paper.length > 0
    readonly property color textInk: root.textToned ? root.textTone.ink : AppTheme.colourText
    readonly property string textFont: AppReaderSettings.textFont.length > 0 ? AppReaderSettings.textFont
                                                                             : AppMetrics.fontFamily
    readonly property bool contentsLoading: reader.isPdf && pdfView.outlineLoading

    function percentOf(index)
    {
        return root.positionCount > 1 ? Math.round(100 * index / (root.positionCount - 1)) : 0
    }

    function goToPosition(index)
    {
        if (reader.isPdf)
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
            reader.downloadFreeBook(document.freeId)

            return
        }

        if (!document.local)
        {
            reader.downloadBook(document.cloudId)

            return
        }

        reader.openDocument(document.id)
    }

    function confirmRemove(document)
    {
        const inCloud = document.cloudId.length > 0

        confirmSheet.ask(inCloud ? qsTr("Remove from this phone?") : qsTr("Remove from library?"),
                         inCloud ? qsTr("\"%1\" stays in the cloud, and you can download it again.").arg(document.title)
                                 : document.free ? qsTr("\"%1\" leaves the library. You can download it again from Free books.").arg(document.title)
                                                 : qsTr("\"%1\" leaves the library. The file stays on your phone.").arg(document.title),
                         qsTr("Remove"),
                         function() { reader.removeDocument(document.id) })
    }

    function confirmCloudDelete(document)
    {
        confirmSheet.ask(qsTr("Delete from the cloud?"),
                         qsTr("\"%1\" is removed from the cloud and from every device that downloaded it.").arg(document.title),
                         qsTr("Delete"),
                         function() { reader.deleteCloudBook(document.cloudId) })
    }

    function openExternal(path, name)
    {
        if (reader.reading)
        {
            root.closeBook()
        }

        reader.openExternal(path, name)
    }

    function turnPage(forward)
    {
        if (reader.isPdf)
        {
            pdfView.goToPage(pdfView.pageIndex + (forward ? 1 : -1))

            return
        }

        readingList.leaveReadAloud()

        const step = readingList.height * 0.9 * (forward ? 1 : -1)
        const bottom = Math.max(0, readingList.contentHeight - readingList.height)

        readingList.contentY = Math.max(0, Math.min(bottom, readingList.contentY + step))
    }

    function toggleChrome()
    {
        reader.fullScreen = !reader.fullScreen
    }

    function openLookup(term, focusField, sentence, place)
    {
        root.lookupPlace = place || null
        lookupSheet.openFor(term, focusField, sentence)
    }

    function toggleReadAloud()
    {
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
        readingList.playReadAloud()
    }

    function closeLookup()
    {
        lookupSheet.close()
    }

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

        if (bookSheet.visible)
        {
            bookSheet.close()

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

        if (reader.isPdf)
        {
            pdfView.requestOutline()
        }
    }

    function refresh()
    {
        if (!reader.reading)
        {
            reader.refresh()

            if (root.books.length === 0)
            {
                root.freeShown = root.freeAvailable
            }
        }
    }

    function closeBook()
    {
        if (reader.isPdf)
        {
            reader.savePosition(root.pdfPendingPage)
            reader.saveZoom(root.pdfPendingZoom)
            reader.saveScroll(root.pdfPendingScroll)
        }
        else
        {
            reader.savePosition(root.topParagraph)
        }

        root.closeLookup()
        root.outlineOpen = false
        root.settingsOpen = false

        reader.closeDocument()
        reader.refresh()
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

    ReaderController
    {
        id: reader

        onFreeBookFailed: function(message) { toast.show(message) }
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
            readingList.playReadAloud()
        }
    }

    Connections
    {
        target: reader

        function onCurrentDocumentChanged()
        {
            root.voiceNoticeShown = false
            root.pdfPendingPage = reader.currentPosition
            root.pdfPendingZoom = reader.currentZoom
            root.pdfPendingScroll = reader.currentScroll
        }

        function onVolumePageTurn(forward)
        {
            root.turnPage(forward)
        }

        function onLoadingChanged()
        {
            if (reader.reading && !reader.loading && !reader.isPdf)
            {
                Qt.callLater(function()
                {
                    readingList.positionViewAtIndex(reader.currentPosition, ListView.Beginning)
                })
            }
        }
    }

    Component.onCompleted: root.refresh()

    MobileShell
    {
        id: libraryShell

        anchors.fill: parent

        activeTab: "reader"
        headerTitle: root.reading ? reader.currentTitle : qsTr("Reader")
        showBack: root.reading || root.searchOpen
        showTabBar: !root.reading
        showHeader: !root.reading
        showSearch: !root.searchOpen
        showSearchField: root.searchOpen

        onSearchRequested:
        {
            root.searchOpen = true
            Qt.callLater(libraryShell.focusSearchField)
        }
        onSearchFieldAccepted: root.forceActiveFocus()

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
            visible: !root.reading

            Flickable
            {
                id: libraryPage

                anchors.fill: parent
                contentWidth: width
                contentHeight: libraryColumn.implicitHeight + AppMetrics.contentPadding * 2
                boundsBehavior: Flickable.StopAtBounds
                clip: true

                ColumnLayout
                {
                    id: libraryColumn

                    x: AppMetrics.contentPadding
                    y: AppMetrics.contentPadding
                    width: libraryPage.width - AppMetrics.contentPadding * 2
                    spacing: AppMetrics.gapCompact

                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: AppMetrics.gapSmall

                        MobileSettingsSelect
                        {
                            objectName: "reader_filter"

                            minimumWidth: 0
                            label: !root.freeShown ? qsTr("My books")
                                : root.levelFilter.length > 0 ? Shelf.levelLabel(root.levelFilter) : qsTr("Free books")
                            Accessible.role: Accessible.Button
                            Accessible.name: qsTr("Filter books: %1").arg(label)

                            onClicked: levelPicker.open(qsTr("Filter books"),
                                [qsTr("My books"), qsTr("Free books"), qsTr("Beginner"),
                                 qsTr("Intermediate"), qsTr("Advanced")],
                                !root.freeShown ? 0 : Shelf.levels.indexOf(root.levelFilter) + 1,
                                function(index)
                                {
                                    root.freeShown = index > 0
                                    root.levelFilter = index > 0 ? Shelf.levels[index - 1] : ""
                                    if (root.freeShown)
                                    {
                                        reader.refreshFreeBooks()
                                    }
                                },
                                ["", qsTr("All levels"), qsTr("Free books"),
                                 qsTr("Free books"), qsTr("Free books")])
                        }

                        MobileSettingsSelect
                        {
                            objectName: "reader_sort"

                            minimumWidth: 0
                            label: root.sortOrder === "title" ? qsTr("A to Z") : qsTr("Last opened")
                            Accessible.role: Accessible.Button
                            Accessible.name: qsTr("Sort books: %1").arg(label)

                            onClicked: levelPicker.open(qsTr("Sort books"),
                                [qsTr("Last opened"), qsTr("A to Z")], root.sortOrder === "title" ? 1 : 0,
                                function(index) { root.sortOrder = index === 1 ? "title" : "recent" })
                        }

                        Item
                        {
                            Layout.fillWidth: true
                        }

                        Item
                        {
                            objectName: "reader_layout_switch"

                            Layout.preferredWidth: AppMetrics.chipHeight
                            Layout.preferredHeight: AppMetrics.chipHeight

                            Accessible.role: Accessible.Button
                            Accessible.name: root.listLayout ? qsTr("Show as covers") : qsTr("Show as a list")

                            MobileVectorIcon
                            {
                                anchors.centerIn: parent
                                width: AppMetrics.headerActionIconSize + 2
                                height: width

                                svgPath: root.listLayout ? Icons.Icons.layout_grid.path : Icons.Icons.contents.path
                                color: AppTheme.colourText
                            }

                            TapHandler
                            {
                                margin: AppMetrics.gapSmall

                                onTapped: AppViewLayouts.setList("reader", !root.listLayout)
                            }
                        }
                    }

                    MobileCoverGrid
                    {
                        id: coverGrid

                        listLayout: root.listLayout

                        Accessible.role: Accessible.List
                        Accessible.name: qsTr("Books")

                        MobileReaderOpen
                        {
                            objectName: "reader_open_tile"

                            width: coverGrid.tileWidth
                            compact: root.listLayout
                            visible: !root.freeShown && root.searchTerm.length === 0

                            onClicked: reader.pickAndOpen()
                        }

                        Repeater
                        {
                            model: root.shelf

                            delegate: MobileReaderBook
                            {
                                id: bookTile

                                required property var modelData
                                required property int index

                                objectName: "reader_book_" + index

                                width: coverGrid.tileWidth
                                compact: root.listLayout

                                title: modelData.title
                                level: modelData.level || ""
                                format: modelData.format
                                local: modelData.local
                                inCloud: modelData.cloudId.length > 0
                                free: modelData.free || modelData.freeId !== undefined
                                size: modelData.size || 0
                                readProgress: modelData.progress
                                progress:
                                {
                                    const value = !modelData.local && modelData.freeId !== undefined
                                                ? reader.freeTransfers[modelData.freeId]
                                                : reader.transfers[modelData.local ? modelData.id : modelData.cloudId]

                                    return value === undefined ? -1 : value
                                }

                                onClicked: root.openShelfBook(modelData, bookTile.transferring)
                                onActionsRequested: bookSheet.open(modelData)
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

                    Text
                    {
                        Layout.fillWidth: true
                        visible: root.shelf.length === 0 && (root.searchTerm.length > 0 || (root.freeShown && root.levelFilter.length > 0))

                        text: root.searchTerm.length > 0 ? qsTr("No books match \"%1\"").arg(root.searchTerm)
                                                        : qsTr("No books at this level.")
                        color: AppTheme.colourMuted
                        font.pixelSize: AppMetrics.fontBody
                        font.family: AppMetrics.fontFamily
                        wrapMode: Text.WordWrap
                    }
                }
            }
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
                    RowLayout
                    {
                        Layout.fillWidth: true
                        Layout.leftMargin: AppMetrics.contentPadding
                        Layout.rightMargin: AppMetrics.contentPadding
                        Layout.topMargin: AppMetrics.gapSmall
                        visible: root.chromeVisible
                        spacing: AppMetrics.gapSmall

                        MobileActionIconButton
                        {
                            iconSvg: Icons.Icons.chevron_left.path

                            onClicked: root.closeBook()
                        }

                        Item
                        {
                            Layout.fillWidth: true
                        }

                        MobileActionIconButton
                        {
                            objectName: "reader_read_aloud"

                            visible: !reader.isPdf
                            active: reader.readAloud.started || root.voiceNoticeShown
                            iconSvg: Icons.Icons.headphones.path

                            onClicked: root.toggleReadAloud()
                        }

                        MobileActionIconButton
                        {
                            iconSvg: Icons.Icons.contents.path

                            onClicked: root.openOutline()
                        }

                        MobileActionIconButton
                        {
                            iconSvg: Icons.Icons.text_size.path
                            iconMinX: Icons.Icons.text_size.minX
                            iconMinY: Icons.Icons.text_size.minY
                            iconWidth: Icons.Icons.text_size.width
                            iconHeight: Icons.Icons.text_size.height

                            onClicked: root.settingsOpen = true
                        }

                        MobileActionIconButton
                        {
                            iconSvg: Icons.Icons.search.path

                            onClicked: root.openLookup("", true)
                        }
                    },
                    MobileHeader
                    {
                        title: reader.currentTitle
                    }
                ]

                bottomContent: [
                    Rectangle
                    {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 43
                        visible: reader.isPdf

                        radius: AppMetrics.radiusMedium
                        color: AppTheme.colourPanel
                        border.width: AppMetrics.borderWidth
                        border.color: AppTheme.colourBorder

                        MobileActionIconButton
                        {
                            anchors.left: parent.left
                            anchors.leftMargin: AppMetrics.gapSmall
                            anchors.verticalCenter: parent.verticalCenter
                            width: 32
                            height: 32
                            visible: reader.isPdf

                            iconSvg: Icons.Icons.chevron_left.path
                            interactive: pdfView.pageIndex > 0

                            onClicked: pdfView.goToPage(pdfView.pageIndex - 1)
                        }

                        MobileReaderPageNumber
                        {
                            id: pageNumber

                            anchors.centerIn: parent
                            width: Math.min(180, parent.width - 96)
                            height: parent.height
                            page: pdfView.pageIndex
                            pageCount: pdfView.pageCount
                            editable: reader.isPdf
                            text: reader.isPdf
                                ? (pdfView.pageIndex + 1) + " / " + Math.max(pdfView.pageCount, 1)
                                : qsTr("%1%").arg(root.percentOf(root.topParagraph))

                            onPageRequested: function(page) { pdfView.goToPage(page) }
                        }

                        MobileActionIconButton
                        {
                            anchors.right: parent.right
                            anchors.rightMargin: AppMetrics.gapSmall
                            anchors.verticalCenter: parent.verticalCenter
                            width: 32
                            height: 32
                            visible: reader.isPdf

                            iconSvg: Icons.Icons.chevron_right.path
                            interactive: pdfView.pageIndex + 1 < pdfView.pageCount

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
                            visible: !reader.isPdf
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

                        Item
                        {
                            id: scrubber

                            Layout.fillWidth: true
                            Layout.preferredHeight: thumb.height

                            property int previewIndex: -1

                            readonly property int shownIndex: scrubber.previewIndex >= 0
                                ? scrubber.previewIndex : root.positionIndex
                            readonly property real fraction: root.positionCount > 1
                                ? scrubber.shownIndex / (root.positionCount - 1) : 0

                            function indexFromRatio(ratio)
                            {
                                const clamped = Math.max(0, Math.min(1, ratio))
                                return Math.round(clamped * (root.positionCount - 1))
                            }

                            Rectangle
                            {
                                anchors.verticalCenter: parent.verticalCenter
                                width: parent.width
                                height: AppMetrics.borderWidth * 2
                                radius: height / 2
                                color: AppTheme.colourBorder
                            }

                            Rectangle
                            {
                                anchors.verticalCenter: parent.verticalCenter
                                width: thumb.x + thumb.width / 2
                                height: AppMetrics.borderWidth * 2
                                radius: height / 2
                                color: AppTheme.colourAccent
                            }

                            Rectangle
                            {
                                id: thumb

                                x: (scrubber.width - width) * scrubber.fraction
                                anchors.verticalCenter: parent.verticalCenter

                                width: 16
                                height: 16
                                radius: 8
                                color: AppTheme.colourPanel
                                border.width: AppMetrics.borderWidth
                                border.color: AppTheme.colourAccent
                            }

                            Rectangle
                            {
                                visible: scrubberArea.pressed
                                z: 1

                                x: Math.max(0, Math.min(scrubber.width - width, thumb.x + thumb.width / 2 - width / 2))
                                y: thumb.y - height - AppMetrics.gapSmall

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
                                    text: reader.isPdf
                                        ? scrubber.shownIndex + 1
                                        : qsTr("%1%").arg(root.percentOf(scrubber.shownIndex))
                                    font.pixelSize: AppMetrics.fontMeta
                                    font.family: AppMetrics.fontFamily
                                    color: AppTheme.colourText
                                }
                            }

                            MouseArea
                            {
                                id: scrubberArea

                                anchors.fill: parent
                                anchors.margins: -12
                                preventStealing: true

                                onPressed: function(mouse)
                                {
                                    scrubber.previewIndex = scrubber.indexFromRatio(mouse.x / scrubber.width)
                                }
                                onPositionChanged: function(mouse)
                                {
                                    if (scrubberArea.pressed)
                                    {
                                        scrubber.previewIndex = scrubber.indexFromRatio(mouse.x / scrubber.width)
                                    }
                                }
                                onReleased:
                                {
                                    if (scrubber.previewIndex >= 0)
                                    {
                                        root.goToPosition(scrubber.previewIndex)
                                    }

                                    scrubber.previewIndex = -1
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
                        visible: reader.isPdf

                        source: reader.isPdf ? reader.currentPath : ""
                        initialPage: reader.currentPosition
                        initialZoom: reader.currentZoom
                        initialScroll: reader.currentScroll
                        cropMargins: reader.cropMargins
                        paperColour: root.pageTone.paper
                        inkColour: root.pageTone.ink

                        onWordPicked: function(word, sentence) { root.openLookup(word, false, sentence) }
                        onDoubleTapped: root.closeLookup()
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
                        visible: !reader.isPdf && root.textToned
                        color: root.textToned ? root.textTone.paper : "transparent"
                    }

                    ListView
                    {
                        id: readingList

                        anchors.fill: parent
                        spacing: readingList.styled ? 0 : Math.round(reader.textSize * 0.6)
                        clip: true
                        visible: !reader.isPdf

                        readonly property bool styled: reader.paragraphModel !== null
                            && reader.paragraphModel !== undefined && reader.paragraphModel.styled === true

                        model: reader.paragraphModel

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
                            ReadAloudPlace.play(readingList, reader.readAloud, root.topParagraph,
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
                            reader.readAloud.startAt(place.paragraph, place.character)
                        }

                        function showReading()
                        {
                            readingList.followingReadAloud = true
                            readingList.followReadAloud(true)
                        }

                        function updateReadingAway()
                        {
                            readingList.readingAway = reader.readAloud.started && !readingList.followingReadAloud
                                && !ReadAloudPlace.isReadingShown(readingList, reader.readAloud,
                                                                  readingList.contentY + readingList.readAloudTop,
                                                                  readingList.contentY + readingList.height - readingList.readAloudBottom)
                        }

                        function followReadAloud(force)
                        {
                            if (!reader.readAloud.started || !readingList.followingReadAloud
                                || (!force && !reader.readAloud.playing))
                            {
                                return
                            }

                            let span = ReadAloudPlace.sentenceSpan(readingList, reader.readAloud)

                            if (span === null)
                            {
                                followGlide.stop()
                                readingList.positionViewAtIndex(reader.readAloud.paragraph, ListView.Beginning)
                                readingList.forceLayout()
                                span = ReadAloudPlace.sentenceSpan(readingList, reader.readAloud)

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
                            followGlide.to = Math.max(first, Math.min(last, span.top - readingList.readAloudTop - reader.textSize * 2))
                            followGlide.start()
                        }

                        onMovementStarted: readingList.leaveReadAloud()

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
                            target: reader.readAloud

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
                                    readingList.clearSelection()
                                    root.toggleChrome()
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
                            readonly property real textWidth: paragraph.width - reader.textMargin * 2
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
                                        paragraph.previousMarginBottom * reader.textSize
                                            + paragraph.previousMarginBottomWidth * paragraph.textWidth,
                                        paragraph.marginTop * reader.textSize
                                            + paragraph.marginTopWidth * paragraph.textWidth)
                                }

                                return paragraph.heading > 0 ? reader.textSize : 0
                            }

                            width: ListView.view.width
                            height: (paragraph.pictured ? picture.height
                                                        : (paragraph.rule ? reader.textSize : paragraphText.implicitHeight))
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
                                x: reader.textMargin + (paragraph.textWidth - width) / 2
                                y: paragraph.topGap + (reader.textSize - height) / 2
                                width: Math.round(paragraph.textWidth / 3)
                                height: AppMetrics.borderWidth
                                color: root.textInk
                                opacity: 0.4
                            }

                            Image
                            {
                                id: picture

                                visible: paragraph.pictured
                                x: reader.textMargin + (paragraph.textWidth - width) / 2
                                y: paragraph.topGap
                                width: paragraph.imageWidth > 0
                                    ? Math.round(paragraph.imageWidth * paragraph.pictureScale)
                                    : Math.min(paragraph.textWidth, implicitWidth)
                                height: paragraph.imageHeight > 0
                                    ? Math.round(paragraph.imageHeight * paragraph.pictureScale)
                                    : (implicitWidth > 0 ? width * implicitHeight / implicitWidth : 0)
                                source: paragraph.pictured
                                    ? "image://book/" + encodeURIComponent(reader.currentPath) + "/"
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
                                x: reader.textMargin
                                y: paragraph.topGap
                                width: parent.width - reader.textMargin * 2

                                text: "<p" + (paragraph.textAlign.length > 0 ? " align='" + paragraph.textAlign + "'" : "")
                                    + " style='margin:0; line-height:" + reader.lineSpacing + "%"
                                    + "; margin-left:" + Math.round(paragraph.marginLeft * reader.textSize
                                                                    + paragraph.marginLeftWidth * paragraph.textWidth) + "px"
                                    + "; margin-right:" + Math.round(paragraph.marginRight * reader.textSize
                                                                     + paragraph.marginRightWidth * paragraph.textWidth) + "px"
                                    + (paragraph.textIndent !== 0 || paragraph.textIndentWidth !== 0
                                       ? "; text-indent:" + Math.round(paragraph.textIndent * reader.textSize
                                                                       + paragraph.textIndentWidth * paragraph.textWidth) + "px"
                                       : "")
                                    + "'>"
                                    + (paragraph.markup.length > 0 ? paragraph.markup : root.escapeHtml(paragraph.display))
                                    + "</p>"
                                textFormat: TextEdit.RichText
                                color: root.textInk
                                font.pixelSize: Math.round(reader.textSize * (readingList.styled
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
                                    readonly property bool reading: reader.readAloud.started
                                                                    && reader.readAloud.paragraph === paragraph.index

                                    textDocument: reader.vocabulary.enabled || reading ? paragraphText.textDocument : null
                                    vocabulary: reader.vocabulary
                                    darkPaper: root.textInk.hslLightness > 0.5
                                    learningColour: darkPaper ? AppTheme.colourReaderMarkDark : AppTheme.colourAccent
                                    sentenceStart: reading ? reader.readAloud.sentenceStart : -1
                                    sentenceEnd: reading ? reader.readAloud.sentenceEnd : -1
                                    sentenceColour: AppTheme.faded(root.textInk, AppTheme.alphaSoft)
                                }

                                TapHandler
                                {
                                    longPressThreshold: 0.4

                                    onTapped: function(eventPoint)
                                    {
                                        const index = hitTest.characterAt(
                                            eventPoint.position.x, eventPoint.position.y)
                                        const range = root.wordRange(paragraph.display, index)

                                        if (range === null)
                                        {
                                            readingList.clearSelection()
                                            root.toggleChrome()

                                            return
                                        }

                                        reader.readAloud.pause()
                                        readingList.claimSelection(paragraphText)
                                        paragraphText.select(range.start, range.end)
                                        root.openLookup(paragraph.display.substring(range.start, range.end),
                                                        false, lookupSheet.dictionary.sentenceAt(paragraph.display, range.start),
                                                        { paragraph: paragraph.index, character: range.start })
                                    }

                                    onLongPressed:
                                    {
                                        const index = hitTest.characterAt(
                                            selectionTracker.point.position.x,
                                            selectionTracker.point.position.y)
                                        const range = root.wordRange(paragraph.display, index)

                                        if (range === null)
                                        {
                                            return
                                        }

                                        reader.readAloud.pause()
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
                }
            }

            MobileReaderPlayBar
            {
                id: playBar

                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: readingFrame.bottomCover + AppMetrics.gapMedium
                                      + (readingFrame.controlsVisible ? 0 : readingFrame.SafeArea.margins.bottom)
                visible: !reader.isPdf && !reader.loading && reader.readAloud.started

                readAloud: reader.readAloud

                onPlayRequested: readingList.playReadAloud()
            }

            Rectangle
            {
                objectName: "reader_read_aloud_back"

                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: playBar.top
                anchors.bottomMargin: AppMetrics.gapSmall
                visible: playBar.visible && readingList.readingAway
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

                    onClicked: readingList.showReading()
                }
            }

            MobileReaderVoiceNotice
            {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: playBar.anchors.bottomMargin
                visible: root.voiceNoticeShown && !reader.isPdf && !reader.loading && !reader.readAloud.available

                readAloud: reader.readAloud

                onClosed: root.voiceNoticeShown = false
            }

            LoadingScreen
            {
                anchors.fill: parent
                visible: reader.loading || (reader.isPdf && !pdfView.presented)
                message: qsTr("Opening book")
            }

            Timer
            {
                id: positionSaveTimer
                interval: 800

                onTriggered:
                {
                    if (reader.reading && reader.isPdf)
                    {
                        reader.savePosition(root.pdfPendingPage)
                        reader.saveZoom(root.pdfPendingZoom)
                        reader.saveScroll(root.pdfPendingScroll)
                    }
                    else if (reader.reading && !reader.loading)
                    {
                        reader.savePosition(root.topParagraph)
                    }
                }
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
                        const id = reader.currentId
                        root.closeBook()
                        reader.removeDocument(id)
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

        anchors.fill: parent
        readAloudOffered: !reader.isPdf && reader.readAloud.available && root.lookupPlace !== null

        onAddWordRequested: function(word, translation, partOfSpeech, hint)
        {
            root.addWordRequested(word, translation, partOfSpeech, hint)
        }

        onReadAloudRequested:
        {
            const place = root.lookupPlace

            lookupSheet.close()
            root.voiceNoticeShown = false
            readingList.readFrom(place)
        }

        onEntryOpened: function(headword, partOfSpeech, hint) { root.entryOpened(headword, partOfSpeech, hint) }
    }

    MobileBottomSheet
    {
        anchors.fill: parent
        z: 50
        visible: root.outlineOpen

        heading: qsTr("Contents")
        maxHeightFraction: 0.85

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
                    text: reader.isPdf
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
        maxHeightFraction: 0.85

        onDismissed: root.settingsOpen = false

        overlays: MobileSettingsPicker
        {
            id: tonePicker
        }

        MobileSettingsReader
        {
            optionPicker: tonePicker
            showPdf: reader.isPdf
            showText: !reader.isPdf
            titled: false
            readAloud: reader.isPdf ? null : reader.readAloud
        }
    }

    MobileReaderBookSheet
    {
        id: bookSheet

        cloudAvailable: reader.cloudAvailable

        onDownloadRequested: function(book) { reader.downloadBook(book.cloudId) }
        onUploadRequested: function(book) { reader.uploadDocument(book.id) }
        onShowRequested: function(book) { reader.showOnDevice(book.id) }
        onRemoveRequested: function(book) { root.confirmRemove(book) }
        onCloudDeleteRequested: function(book) { root.confirmCloudDelete(book) }
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
