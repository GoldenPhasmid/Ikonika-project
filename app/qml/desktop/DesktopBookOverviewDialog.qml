import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../ReaderShelf.js" as Shelf

Popup
{
    id: root

    property var controller: null
    property int maxStage: 1
    property string documentId: ""
    property int tab: 0
    property bool renaming: false
    property var afterClose: null

    readonly property var book:
    {
        if (!root.controller)
        {
            return null
        }

        for (const document of root.controller.documents)
        {
            if (document.id === root.documentId)
            {
                return document
            }
        }

        return null
    }

    readonly property var details: root.controller && root.controller.details.id === root.documentId ? root.controller.details : ({})
    readonly property bool exists: root.book !== null && root.book.exists
    readonly property bool inCloud: root.book !== null && root.book.cloudId.length > 0
    readonly property bool free: root.book !== null && (root.book.free || root.book.freeId !== undefined)
    readonly property real readProgress: root.book !== null ? Math.max(0, root.book.progress) : 0
    readonly property var chapters: root.details.chapters || []
    readonly property bool loading: root.details.loading === true
    readonly property int addedWords: root.book !== null && root.book.addedWords !== undefined ? root.book.addedWords : 0
    readonly property var words: root.tab === 2 && root.book !== null && root.controller
                                 ? root.controller.bookWords(root.book.key) : []

    signal readRequested(var book)
    signal chapterRequested(var book, int position)
    signal newCollectionRequested(string key)

    function show(documentId)
    {
        root.documentId = documentId
        root.tab = 0
        root.renaming = false
        root.controller.loadDetails(documentId)
        root.open()
    }

    function closeThen(action)
    {
        root.afterClose = action
        root.close()
    }

    onClosed:
    {
        const action = root.afterClose

        root.afterClose = null

        if (action)
        {
            action()
        }
    }

    function startRename()
    {
        titleField.text = root.book.title
        root.renaming = true
        titleField.forceActiveFocus()
        titleField.selectAll()
    }

    function finishRename()
    {
        if (!root.renaming)
        {
            return
        }

        root.renaming = false

        const title = titleField.text.trim()

        if (title.length > 0 && title !== root.book.title)
        {
            root.controller.renameDocument(root.documentId, title)
        }
    }

    function count(value)
    {
        if (root.loading || value === undefined)
        {
            return "—"
        }

        return value >= 10000 ? qsTr("%1k").arg(Math.round(value / 1000)) : value.toString()
    }

    onBookChanged:
    {
        if (root.opened && root.book === null)
        {
            root.close()
        }
    }

    parent: Overlay.overlay
    anchors.centerIn: parent

    width: Math.min(AppMetrics.bookOverviewWidth, parent ? parent.width - AppMetrics.contentPadding * 2 : AppMetrics.bookOverviewWidth)
    height: Math.min(AppMetrics.bookOverviewHeight, parent ? parent.height - AppMetrics.contentPadding * 2 : AppMetrics.bookOverviewHeight)
    padding: AppMetrics.contentPadding

    modal: true
    focus: true
    closePolicy: root.renaming ? Popup.CloseOnPressOutside : Popup.CloseOnEscape | Popup.CloseOnPressOutside

    Overlay.modal: Rectangle
    {
        color: AppTheme.colourScrim

        Behavior on opacity { NumberAnimation { duration: AppMetrics.motionBase } }
    }

    background: Rectangle
    {
        radius: AppMetrics.radiusLarge
        color: AppTheme.colourPanel
        border.width: AppMetrics.borderWidth
        border.color: AppTheme.colourBorder
    }

    enter: Transition
    {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: AppMetrics.motionBase }
        NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: AppMetrics.motionBase; easing.type: AppMetrics.motionEasing }
    }

    exit: Transition
    {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: AppMetrics.motionBase }
        NumberAnimation { property: "scale"; from: 1; to: 0.96; duration: AppMetrics.motionBase; easing.type: AppMetrics.motionEasing }
    }

    contentItem: Item
    {
        Accessible.role: Accessible.Dialog
        Accessible.name: root.book ? root.book.title : ""

        DesktopIconButton
        {
            id: closeButton

            objectName: "book_overview_close"

            anchors.right: parent.right
            anchors.top: parent.top
            anchors.rightMargin: -AppMetrics.gapMedium
            anchors.topMargin: (AppMetrics.buttonHeight - closeButton.height) / 2
            z: 1
            iconName: "close"
            text: qsTr("Close")

            onClicked: root.close()
        }

        RowLayout
        {
            anchors.fill: parent
            anchors.rightMargin: closeButton.width
            spacing: AppMetrics.contentPadding
            visible: root.book !== null

            ColumnLayout
            {
                Layout.preferredWidth: AppMetrics.bookOverviewCoverWidth
                Layout.maximumWidth: AppMetrics.bookOverviewCoverWidth
                Layout.fillHeight: true
                spacing: AppMetrics.gapSmall

                DesktopBookCover
                {
                    Layout.preferredWidth: AppMetrics.bookOverviewCoverWidth
                    Layout.preferredHeight: Math.round(AppMetrics.bookOverviewCoverWidth * AppMetrics.bookCoverAspect)
                    Layout.bottomMargin: AppMetrics.gapSmall
                    title: root.book ? root.book.title : ""
                    author: root.book ? root.book.author || "" : ""
                    imageSource: root.book ? root.book.cover || "" : ""
                    missing: !root.exists
                }

                DesktopButton
                {
                    objectName: "book_overview_read"

                    Layout.fillWidth: true
                    Layout.bottomMargin: AppMetrics.gapSmall
                    tone: "primary"
                    large: true
                    iconName: "nav_reader"
                    enabled: root.exists
                    text: root.readProgress > 0 ? qsTr("Continue reading") : qsTr("Start reading")

                    onClicked: root.closeThen(function() { root.readRequested(root.book) })
                }

                DesktopButton
                {
                    Layout.fillWidth: true
                    visible: root.exists
                    iconName: "external_link"
                    text: qsTr("Show in folder")

                    onClicked: root.controller.showOnDevice(root.documentId)
                }

                DesktopButton
                {
                    objectName: "book_overview_upload"

                    Layout.fillWidth: true
                    visible: root.exists && !root.inCloud && !root.free && root.controller.cloudAvailable
                    iconName: "cloud_upload"
                    text: qsTr("Upload to the cloud")

                    onClicked: root.controller.uploadDocument(root.documentId)
                }

                DesktopHoldButton
                {
                    objectName: "book_overview_cloud_delete"

                    Layout.fillWidth: true
                    visible: root.inCloud && root.controller.cloudAvailable
                    iconName: "cloud_off"
                    label: qsTr("Delete from the cloud")
                    holdLabel: qsTr("Hold to delete from the cloud")

                    onConfirmed: root.controller.deleteCloudBook(root.book.cloudId)
                }

                DesktopHoldButton
                {
                    objectName: "book_overview_remove"

                    Layout.fillWidth: true
                    iconName: "trash"
                    label: root.inCloud ? qsTr("Remove from this device") : qsTr("Remove from library")
                    holdLabel: qsTr("Hold to remove")

                    onConfirmed:
                    {
                        const documentId = root.documentId

                        root.closeThen(function() { root.controller.removeDocument(documentId) })
                    }
                }

                Item
                {
                    Layout.fillHeight: true
                }
            }

            ColumnLayout
            {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                RowLayout
                {
                    Layout.fillWidth: true
                    spacing: AppMetrics.gapSmall

                    DesktopLabel
                    {
                        objectName: "book_overview_title"

                        Layout.fillWidth: true
                        visible: !root.renaming
                        text: root.book ? root.book.title : ""
                        font.pixelSize: AppMetrics.fontHeadline
                        font.weight: Font.DemiBold
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        elide: Text.ElideRight

                        Accessible.role: Accessible.Heading
                        Accessible.name: text
                    }

                    DesktopTextField
                    {
                        id: titleField

                        objectName: "book_overview_title_field"

                        Layout.fillWidth: true
                        visible: root.renaming
                        large: true

                        Accessible.name: qsTr("Book title")

                        onAccepted: root.finishRename()
                        onActiveFocusChanged:
                        {
                            if (!activeFocus)
                            {
                                root.finishRename()
                            }
                        }

                        Keys.onEscapePressed: function(event)
                        {
                            root.renaming = false
                            event.accepted = true
                        }
                    }

                    DesktopButton
                    {
                        objectName: "book_overview_rename"

                        Layout.alignment: Qt.AlignTop
                        visible: !root.renaming
                        iconOnly: true
                        iconName: "edit"
                        text: qsTr("Rename")

                        Accessible.name: text

                        onClicked: root.startRename()
                    }

                    DesktopButton
                    {
                        objectName: "book_overview_favorite"

                        readonly property bool on: root.book !== null && root.book.favorite

                        Layout.alignment: Qt.AlignTop
                        iconOnly: true
                        iconName: "heart"
                        iconFilled: on
                        iconColour: on ? AppTheme.colourWrong : AppTheme.colourText
                        text: on ? qsTr("Remove from favorites") : qsTr("Add to favorites")

                        Accessible.name: text

                        onClicked: root.controller.setFavorite(root.book.key, !on)
                    }
                }

                DesktopLabel
                {
                    Layout.fillWidth: true
                    Layout.topMargin: AppMetrics.gapTight
                    visible: text.length > 0
                    text: root.book ? root.book.author || "" : ""
                    font.pixelSize: AppMetrics.fontLabel
                    color: AppTheme.colourSubtle
                    elide: Text.ElideRight
                }

                Row
                {
                    Layout.topMargin: AppMetrics.gapMedium
                    spacing: AppMetrics.gapSmall

                    Rectangle
                    {
                        visible: root.book !== null && Shelf.levelLabel(root.book.level || "").length > 0
                        implicitWidth: levelRow.implicitWidth + AppMetrics.gapSmall * 2
                        implicitHeight: AppMetrics.badgeHeight + AppMetrics.gapSmall
                        radius: AppMetrics.radiusSmall
                        color: "transparent"
                        border.width: AppMetrics.borderWidth
                        border.color: AppTheme.colourBorder

                        Row
                        {
                            id: levelRow

                            anchors.centerIn: parent
                            spacing: AppMetrics.gapSmall

                            ReaderLevelBadge
                            {
                                anchors.verticalCenter: parent.verticalCenter
                                level: root.book ? root.book.level || "" : ""
                            }

                            DesktopLabel
                            {
                                anchors.verticalCenter: parent.verticalCenter
                                text: root.book ? Shelf.levelLabel(root.book.level || "") : ""
                                font.pixelSize: AppMetrics.fontSmall
                                font.weight: Font.Medium
                                color: AppTheme.colourSubtle
                            }
                        }
                    }

                    Repeater
                    {
                        model: root.book ? [root.book.format, root.details.language || ""].filter(function(fact) { return fact.length > 0 }) : []

                        delegate: Rectangle
                        {
                            required property string modelData

                            implicitWidth: factLabel.implicitWidth + AppMetrics.gapCompact * 2
                            implicitHeight: AppMetrics.badgeHeight + AppMetrics.gapSmall
                            radius: AppMetrics.radiusSmall
                            color: "transparent"
                            border.width: AppMetrics.borderWidth
                            border.color: AppTheme.colourBorder

                            DesktopLabel
                            {
                                id: factLabel

                                anchors.centerIn: parent
                                text: modelData
                                font.pixelSize: AppMetrics.fontSmall
                                font.weight: Font.Medium
                                color: AppTheme.colourSubtle
                            }
                        }
                    }
                }

                Flow
                {
                    Layout.fillWidth: true
                    Layout.topMargin: AppMetrics.gapMedium
                    spacing: AppMetrics.gapSmall

                    Repeater
                    {
                        model: root.controller ? root.controller.collections.filter(function(collection)
                        {
                            return root.book !== null && (root.book.collections || []).indexOf(collection.id) >= 0
                        }) : []

                        delegate: DesktopChip
                        {
                            id: collectionChip

                            required property var modelData

                            text: modelData.name
                            dotColour: AppTheme.coverColours[modelData.colour % AppTheme.coverColours.length]

                            onClicked: collectionMenu.popup(collectionChip, 0, collectionChip.height)
                        }
                    }

                    DesktopChip
                    {
                        id: addCollectionChip

                        objectName: "book_overview_add_collection"

                        iconName: "plus"
                        text: qsTr("Collection")

                        onClicked: collectionMenu.popup(addCollectionChip, 0, addCollectionChip.height)
                    }
                }

                RowLayout
                {
                    Layout.fillWidth: true
                    Layout.topMargin: AppMetrics.gapLarge
                    spacing: AppMetrics.gapMedium

                    DesktopLabel
                    {
                        visible: root.chapters.length > 0
                        text: qsTr("Chapter %1 of %2").arg(root.details.chapter || 1).arg(root.chapters.length)
                        font.pixelSize: AppMetrics.fontSmall
                        font.weight: Font.Medium
                    }

                    DesktopProgressBar
                    {
                        Layout.fillWidth: true
                        progress: root.readProgress
                        fill: AppTheme.colourAccent
                        animated: false
                    }

                    DesktopLabel
                    {
                        text: qsTr("%1%").arg(Math.round(root.readProgress * 100))
                        font.pixelSize: AppMetrics.fontSmall
                        font.weight: Font.Medium
                        font.features: { "tnum": 1 }
                    }
                }

                Item
                {
                    id: stats

                    Layout.fillWidth: true
                    Layout.topMargin: AppMetrics.gapLarge
                    implicitHeight: statRepeater.count > 0 && statRepeater.itemAt(0) ? statRepeater.itemAt(0).implicitHeight : 0

                    Repeater
                    {
                        id: statRepeater

                        model: [
                            { "value": root.count(root.details.pages), "label": qsTr("Pages") },
                            { "value": root.count(root.details.words), "label": qsTr("Words") },
                            { "value": root.addedWords.toString(), "label": qsTr("Added words") },
                            { "value": "—", "label": qsTr("Known vocabulary") }
                        ]

                        delegate: Item
                        {
                            required property var modelData
                            required property int index

                            x: index * stats.width / 4
                            width: stats.width / 4
                            height: stats.height
                            implicitHeight: statColumn.implicitHeight

                            DesktopDivider
                            {
                                width: AppMetrics.borderWidth
                                height: parent.height
                                visible: index > 0
                            }

                            Column
                            {
                                id: statColumn

                                x: index > 0 ? AppMetrics.gapLarge : 0
                                width: parent.width - x - AppMetrics.gapSmall
                                spacing: AppMetrics.gapTight

                                DesktopLabel
                                {
                                    text: modelData.value
                                    font.pixelSize: AppMetrics.fontSubtitle
                                    font.weight: Font.DemiBold
                                    font.features: { "tnum": 1 }
                                    color: modelData.value === "—" ? AppTheme.colourSubtle : AppTheme.colourText
                                }

                                DesktopLabel
                                {
                                    width: parent.width
                                    text: modelData.label
                                    font.pixelSize: AppMetrics.fontSmall
                                    color: AppTheme.colourMuted
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }
                }

                RowLayout
                {
                    Layout.fillWidth: true
                    Layout.topMargin: AppMetrics.gapLarge
                    spacing: AppMetrics.gapLarge

                    Repeater
                    {
                        model: [qsTr("Overview"), qsTr("Contents"), qsTr("Words")]

                        delegate: DesktopTab
                        {
                            required property string modelData
                            required property int index

                            objectName: "book_overview_tab_" + index

                            text: modelData
                            selected: root.tab === index

                            onClicked: root.tab = index
                        }
                    }

                    Item
                    {
                        Layout.fillWidth: true
                    }
                }

                DesktopDivider
                {
                }

                StackLayout
                {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.topMargin: AppMetrics.gapLarge
                    currentIndex: root.tab

                    Flickable
                    {
                        id: overviewFlick

                        contentWidth: width
                        contentHeight: overviewRow.implicitHeight
                        boundsBehavior: Flickable.StopAtBounds
                        clip: true

                        ScrollBar.vertical: DesktopScrollBar {}

                        RowLayout
                        {
                            id: overviewRow

                            width: overviewFlick.width - AppMetrics.scrollBarWidth - AppMetrics.gapSmall
                            spacing: AppMetrics.contentPadding

                            ColumnLayout
                            {
                                Layout.fillWidth: true
                                Layout.preferredWidth: 6
                                Layout.alignment: Qt.AlignTop
                                spacing: AppMetrics.gapSmall

                                DesktopSectionLabel
                                {
                                    text: qsTr("About")
                                }

                                DesktopLabel
                                {
                                    Layout.fillWidth: true
                                    text: root.loading ? "" : (root.details.description || qsTr("This book has no description."))
                                    color: root.details.description ? AppTheme.colourText : AppTheme.colourMuted
                                    wrapMode: Text.Wrap
                                    lineHeight: 1.4
                                }
                            }

                            ColumnLayout
                            {
                                Layout.fillWidth: true
                                Layout.preferredWidth: 5
                                Layout.alignment: Qt.AlignTop
                                spacing: AppMetrics.gapSmall

                                DesktopSectionLabel
                                {
                                    text: qsTr("File")
                                }

                                GridLayout
                                {
                                    Layout.fillWidth: true
                                    columns: 2
                                    columnSpacing: AppMetrics.gapMedium
                                    rowSpacing: AppMetrics.gapSmall

                                    DesktopLabel
                                    {
                                        text: qsTr("Format")
                                        color: AppTheme.colourMuted
                                    }

                                    DesktopLabel
                                    {
                                        Layout.fillWidth: true
                                        text: root.book ? [root.book.format, root.book.size > 0 ? qsTr("%1 MB").arg((root.book.size / 1048576).toFixed(1)) : ""]
                                                          .filter(function(part) { return part.length > 0 }).join("  ·  ") : ""
                                    }

                                    DesktopLabel
                                    {
                                        Layout.alignment: Qt.AlignTop
                                        text: qsTr("Location")
                                        color: AppTheme.colourMuted
                                    }

                                    TextEdit
                                    {
                                        objectName: "book_overview_location"

                                        Layout.fillWidth: true
                                        text: root.book ? root.book.path : ""
                                        color: root.exists ? AppTheme.colourText : AppTheme.colourWrong
                                        font.family: AppMetrics.fontFamily
                                        font.pixelSize: AppMetrics.fontSmall
                                        wrapMode: TextEdit.WrapAnywhere
                                        readOnly: true
                                        selectByMouse: true
                                        persistentSelection: true
                                        cursorVisible: false
                                        selectionColor: AppTheme.colourAccent
                                        selectedTextColor: AppTheme.colourAccentText

                                        Accessible.role: Accessible.StaticText
                                        Accessible.name: text

                                        HoverHandler
                                        {
                                            cursorShape: Qt.IBeamCursor
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Item
                    {
                        ListView
                        {
                            id: contentsList

                            objectName: "book_overview_contents"

                            anchors.fill: parent
                            model: root.chapters
                            clip: true
                            boundsBehavior: Flickable.StopAtBounds
                            visible: root.chapters.length > 0

                            ScrollBar.vertical: DesktopScrollBar {}

                            delegate: AbstractButton
                            {
                                id: chapterRow

                                required property var modelData
                                required property int index

                                readonly property bool current: index + 1 === root.details.chapter

                                objectName: "book_overview_chapter_" + index

                                width: contentsList.width - AppMetrics.scrollBarWidth - AppMetrics.gapSmall
                                implicitHeight: AppMetrics.listRowHeight
                                hoverEnabled: true

                                Accessible.name: modelData.title

                                contentItem: DesktopLabel
                                {
                                    leftPadding: AppMetrics.gapMedium + chapterRow.modelData.level * AppMetrics.gapLarge
                                    rightPadding: AppMetrics.gapMedium
                                    text: chapterRow.modelData.title
                                    font.weight: chapterRow.current ? Font.DemiBold : Font.Normal
                                    color: chapterRow.current || chapterRow.hovered ? AppTheme.colourText : AppTheme.colourSubtle
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                }

                                background: Rectangle
                                {
                                    radius: AppMetrics.radiusSmall
                                    color: chapterRow.hovered ? AppTheme.colourHover : (chapterRow.current ? AppTheme.colourSunken : "transparent")
                                }

                                onClicked:
                                {
                                    const book = root.book
                                    const position = chapterRow.modelData.position

                                    root.closeThen(function() { root.chapterRequested(book, position) })
                                }

                                HoverHandler
                                {
                                    cursorShape: Qt.PointingHandCursor
                                }
                            }
                        }

                        DesktopLabel
                        {
                            anchors.horizontalCenter: parent.horizontalCenter
                            visible: root.chapters.length === 0 && !root.loading
                            text: qsTr("This book has no table of contents.")
                            color: AppTheme.colourMuted
                        }
                    }

                    Item
                    {
                        ListView
                        {
                            id: wordsList

                            objectName: "book_overview_words"

                            anchors.fill: parent
                            model: root.words
                            clip: true
                            boundsBehavior: Flickable.StopAtBounds
                            visible: root.words.length > 0

                            ScrollBar.vertical: DesktopScrollBar {}

                            delegate: DesktopWordRow
                            {
                                required property var modelData

                                width: wordsList.width - AppMetrics.scrollBarWidth - AppMetrics.gapSmall
                                word: modelData.word
                                translation: modelData.translation
                                partOfSpeech: modelData.partOfSpeech
                                gender: modelData.gender
                                stage: modelData.stage
                                completed: modelData.completed
                                maxStage: root.maxStage
                            }
                        }

                        DesktopLabel
                        {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: parent.width
                            visible: root.words.length === 0
                            horizontalAlignment: Text.AlignHCenter
                            text: qsTr("Words you add while reading this book will appear here.")
                            color: AppTheme.colourMuted
                            wrapMode: Text.Wrap
                        }
                    }
                }
            }
        }
    }

    DesktopMenu
    {
        id: collectionMenu

        Instantiator
        {
            model: root.controller ? root.controller.collections : []

            delegate: DesktopMenuItem
            {
                required property var modelData

                readonly property bool member: root.book !== null && (root.book.collections || []).indexOf(modelData.id) >= 0

                text: modelData.name
                iconName: member ? "check" : ""

                onTriggered:
                {
                    if (member)
                    {
                        root.controller.removeFromCollection(modelData.id, root.book.key)

                        return
                    }

                    root.controller.addToCollection(modelData.id, root.book.key)
                }
            }

            onObjectAdded: function(index, object) { collectionMenu.insertItem(index, object) }
            onObjectRemoved: function(index, object) { collectionMenu.removeItem(object) }
        }

        DesktopMenuItem
        {
            text: qsTr("New collection…")
            iconName: "plus"

            onTriggered: root.newCollectionRequested(root.book.key)
        }
    }
}
