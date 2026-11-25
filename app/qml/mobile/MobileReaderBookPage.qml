import QtQuick
import QtQuick.Layouts
import "../ReaderShelf.js" as Shelf

Item
{
    id: root

    property var controller: null
    property string documentId: ""
    property int tab: 0

    readonly property var book:
    {
        if (!root.controller || root.documentId.length === 0)
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
    readonly property var words: root.tab === 2 && root.book !== null && root.controller ? root.controller.bookWords(root.book.key) : []
    readonly property bool mobilePlatform: Qt.platform.os === "android" || Qt.platform.os === "ios"

    signal readRequested(var book)
    signal chapterRequested(var book, int position)
    signal collectionsRequested(var book)
    signal removed()

    function show(documentId)
    {
        root.documentId = documentId
        root.tab = 0
        page.contentY = 0
        root.controller.loadDetails(documentId)
        root.visible = true
    }

    function close()
    {
        root.visible = false
        root.documentId = ""
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
        if (root.visible && root.documentId.length > 0 && root.book === null)
        {
            root.close()
        }
    }

    visible: false

    component SectionLabel: Text
    {
        font.pixelSize: AppMetrics.fontMicro
        font.weight: Font.DemiBold
        font.family: AppMetrics.fontFamily
        font.letterSpacing: AppMetrics.trackingWide
        font.capitalization: Font.AllUppercase
        color: AppTheme.colourMuted
    }

    component Fact: Rectangle
    {
        id: fact

        property alias text: factLabel.text
        property string level: ""

        implicitWidth: factRow.implicitWidth + AppMetrics.gapSmall * 2
        implicitHeight: AppMetrics.chipHeight - AppMetrics.gapSmall
        radius: AppMetrics.radiusSmall
        color: AppTheme.colourFaint

        Row
        {
            id: factRow

            anchors.centerIn: parent
            spacing: AppMetrics.gapTight

            ReaderLevelBadge
            {
                anchors.verticalCenter: parent.verticalCenter
                visible: fact.level.length > 0
                level: fact.level
            }

            Text
            {
                id: factLabel

                anchors.verticalCenter: parent.verticalCenter
                font.pixelSize: AppMetrics.fontBody
                font.weight: Font.Medium
                font.family: AppMetrics.fontFamily
                color: AppTheme.colourSubtle
            }
        }
    }

    Rectangle
    {
        anchors.fill: parent
        color: AppTheme.colourPanel

        MouseArea
        {
            anchors.fill: parent
        }
    }

    Flickable
    {
        id: page

        anchors.fill: parent
        contentWidth: width
        contentHeight: column.implicitHeight + AppMetrics.contentPadding * 2
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        ColumnLayout
        {
            id: column

            x: AppMetrics.contentPadding
            y: AppMetrics.contentPadding
            width: page.width - AppMetrics.contentPadding * 2
            spacing: AppMetrics.gapMedium
            visible: root.book !== null

            RowLayout
            {
                Layout.fillWidth: true
                spacing: AppMetrics.contentPadding

                DesktopBookCover
                {
                    Layout.alignment: Qt.AlignTop
                    Layout.preferredWidth: Math.round(AppMetrics.readerRowCoverWidth * 2)
                    Layout.preferredHeight: Math.round(Math.round(AppMetrics.readerRowCoverWidth * 2) * AppMetrics.bookCoverAspect)
                    title: root.book ? root.book.title : ""
                    author: root.book ? root.book.author || "" : ""
                    imageSource: root.book ? root.book.cover || "" : ""
                    missing: !root.exists
                }

                ColumnLayout
                {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    spacing: AppMetrics.gapTight

                    Text
                    {
                        objectName: "book_page_title"

                        Layout.fillWidth: true
                        text: root.book ? root.book.title : ""
                        font.pixelSize: AppMetrics.fontLead
                        font.weight: Font.DemiBold
                        font.family: AppMetrics.fontFamily
                        color: AppTheme.colourText
                        wrapMode: Text.Wrap
                        maximumLineCount: 3
                        elide: Text.ElideRight

                        Accessible.role: Accessible.Heading
                        Accessible.name: text
                    }

                    Text
                    {
                        Layout.fillWidth: true
                        visible: text.length > 0
                        text: root.book ? root.book.author || "" : ""
                        font.pixelSize: AppMetrics.fontLabel
                        font.family: AppMetrics.fontFamily
                        color: AppTheme.colourMuted
                        elide: Text.ElideRight
                    }

                    Flow
                    {
                        Layout.fillWidth: true
                        Layout.topMargin: AppMetrics.gapSmall
                        spacing: AppMetrics.gapTight

                        Fact
                        {
                            visible: root.book !== null && Shelf.levelLabel(root.book.level || "").length > 0
                            level: root.book ? root.book.level || "" : ""
                            text: root.book ? Shelf.levelLabel(root.book.level || "") : ""
                        }

                        Repeater
                        {
                            model: root.book ? [root.book.format, root.details.language || ""].filter(function(fact) { return fact.length > 0 }) : []

                            delegate: Fact
                            {
                                required property string modelData

                                text: modelData
                            }
                        }
                    }

                    Flow
                    {
                        Layout.fillWidth: true
                        Layout.topMargin: AppMetrics.gapSmall
                        spacing: AppMetrics.gapSmall

                        Repeater
                        {
                            model: root.controller && root.book ? root.controller.collections.filter(function(collection)
                            {
                                return (root.book.collections || []).indexOf(collection.id) >= 0
                            }) : []

                            delegate: MobileFilterChip
                            {
                                required property var modelData

                                label: modelData.name
                                dotColour: AppTheme.coverColours[modelData.colour % AppTheme.coverColours.length]

                                onClicked: root.collectionsRequested(root.book)
                            }
                        }

                        MobileFilterChip
                        {
                            objectName: "book_page_add_collection"

                            label: qsTr("+ Collection")

                            onClicked: root.collectionsRequested(root.book)
                        }
                    }
                }
            }

            RowLayout
            {
                Layout.fillWidth: true
                spacing: AppMetrics.gapCompact

                Text
                {
                    visible: root.chapters.length > 0
                    text: qsTr("Chapter %1 of %2").arg(root.details.chapter || 1).arg(root.chapters.length)
                    font.pixelSize: AppMetrics.fontSmall
                    font.weight: Font.Medium
                    font.family: AppMetrics.fontFamily
                    color: AppTheme.colourSubtle
                }

                Rectangle
                {
                    Layout.fillWidth: true
                    Layout.preferredHeight: AppMetrics.coverProgressHeight
                    radius: height / 2
                    color: AppTheme.colourBorder

                    Rectangle
                    {
                        width: parent.width * root.readProgress
                        height: parent.height
                        radius: parent.radius
                        color: AppTheme.colourAccent
                    }
                }

                Text
                {
                    text: qsTr("%1%").arg(Math.round(root.readProgress * 100))
                    font.pixelSize: AppMetrics.fontSmall
                    font.weight: Font.Medium
                    font.family: AppMetrics.fontFamily
                    font.features: { "tnum": 1 }
                    color: AppTheme.colourSubtle
                }
            }

            MobileWideButton
            {
                objectName: "book_page_read"

                label: root.readProgress > 0 ? qsTr("Continue reading") : qsTr("Start reading")
                interactive: root.exists

                onClicked: root.readRequested(root.book)
            }

            RowLayout
            {
                Layout.fillWidth: true
                spacing: AppMetrics.gapSmall

                MobileActionTile
                {
                    objectName: "book_page_open_with"

                    visible: root.exists && root.mobilePlatform
                    iconName: "external_link"
                    label: qsTr("Open with")

                    onClicked: root.controller.showOnDevice(root.documentId)
                }

                MobileActionTile
                {
                    objectName: "book_page_upload"

                    visible: root.exists && !root.inCloud && !root.free && root.controller.cloudAvailable
                    iconName: "cloud_upload"
                    label: qsTr("Upload")

                    onClicked: root.controller.uploadDocument(root.documentId)
                }

                MobileActionTile
                {
                    objectName: "book_page_cloud_delete"

                    visible: root.inCloud && root.controller.cloudAvailable
                    iconName: "cloud_off"
                    label: qsTr("Hold to delete")
                    destructive: true
                    holdToConfirm: true

                    onConfirmed: root.controller.deleteCloudBook(root.book.cloudId)
                }

                MobileActionTile
                {
                    objectName: "book_page_remove"

                    iconName: "trash"
                    label: qsTr("Hold to remove")
                    destructive: true
                    holdToConfirm: true

                    onConfirmed:
                    {
                        const documentId = root.documentId

                        root.close()
                        root.controller.removeDocument(documentId)
                        root.removed()
                    }
                }
            }

            Rectangle
            {
                Layout.fillWidth: true
                implicitHeight: statRow.implicitHeight + AppMetrics.gapMedium * 2
                radius: AppMetrics.radiusMedium
                color: AppTheme.colourSunken

                RowLayout
                {
                    id: statRow

                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 0

                    Repeater
                    {
                        model: [
                            { "value": root.count(root.details.pages), "label": qsTr("Pages") },
                            { "value": root.addedWords.toString(), "label": qsTr("Added words") },
                            { "value": "—", "label": qsTr("Known vocabulary") }
                        ]

                        delegate: Item
                        {
                            required property var modelData
                            required property int index

                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            implicitHeight: statColumn.implicitHeight

                            Rectangle
                            {
                                visible: index > 0
                                width: AppMetrics.borderWidth
                                height: parent.height
                                color: AppTheme.colourBorder
                            }

                            ColumnLayout
                            {
                                id: statColumn

                                width: parent.width
                                spacing: AppMetrics.gapTight

                                Text
                                {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: modelData.value
                                    font.pixelSize: AppMetrics.fontSubtitle
                                    font.weight: Font.DemiBold
                                    font.family: AppMetrics.fontFamily
                                    font.features: { "tnum": 1 }
                                    color: modelData.value === "—" ? AppTheme.colourMuted : AppTheme.colourText
                                }

                                Text
                                {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: statMetrics.height * 2
                                    text: modelData.label
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                    font.pixelSize: AppMetrics.fontSmall
                                    font.family: AppMetrics.fontFamily
                                    color: AppTheme.colourMuted
                                    wrapMode: Text.WordWrap
                                    maximumLineCount: 2
                                }
                            }
                        }
                    }
                }

                FontMetrics
                {
                    id: statMetrics

                    font.pixelSize: AppMetrics.fontSmall
                    font.family: AppMetrics.fontFamily
                }
            }

            ColumnLayout
            {
                Layout.fillWidth: true
                Layout.topMargin: AppMetrics.gapSmall
                spacing: 0

                RowLayout
                {
                    Layout.fillWidth: true
                    spacing: 0

                    Repeater
                    {
                        model: [qsTr("Overview"),
                                root.chapters.length > 0 ? qsTr("Contents · %1").arg(root.chapters.length) : qsTr("Contents"),
                                root.addedWords > 0 ? qsTr("Words · %1").arg(root.addedWords) : qsTr("Words")]

                        delegate: MobileTab
                        {
                            required property string modelData
                            required property int index

                            objectName: "book_page_tab_" + index

                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            text: modelData
                            selected: root.tab === index

                            onClicked: root.tab = index
                        }
                    }
                }

                Rectangle
                {
                    Layout.fillWidth: true
                    Layout.preferredHeight: AppMetrics.borderWidth
                    color: AppTheme.colourBorder
                }
            }

            ColumnLayout
            {
                Layout.fillWidth: true
                visible: root.tab === 0
                spacing: AppMetrics.gapSmall

                SectionLabel
                {
                    text: qsTr("About")
                }

                Text
                {
                    Layout.fillWidth: true
                    text: root.loading ? "" : (root.details.description || qsTr("This book has no description."))
                    font.pixelSize: AppMetrics.fontBody
                    font.family: AppMetrics.fontFamily
                    color: root.details.description ? AppTheme.colourText : AppTheme.colourMuted
                    wrapMode: Text.Wrap
                    lineHeight: 1.4
                }

                SectionLabel
                {
                    Layout.topMargin: AppMetrics.gapMedium
                    text: qsTr("File")
                }

                GridLayout
                {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: AppMetrics.gapMedium
                    rowSpacing: AppMetrics.gapSmall

                    Text
                    {
                        text: qsTr("Format")
                        font.pixelSize: AppMetrics.fontBody
                        font.family: AppMetrics.fontFamily
                        color: AppTheme.colourMuted
                    }

                    Text
                    {
                        Layout.fillWidth: true
                        text: root.book ? [root.book.format, root.book.size > 0 ? qsTr("%1 MB").arg((root.book.size / 1048576).toFixed(1)) : ""]
                                          .filter(function(part) { return part.length > 0 }).join("  ·  ") : ""
                        font.pixelSize: AppMetrics.fontBody
                        font.family: AppMetrics.fontFamily
                        color: AppTheme.colourText
                    }

                    Text
                    {
                        Layout.alignment: Qt.AlignTop
                        text: qsTr("Location")
                        font.pixelSize: AppMetrics.fontBody
                        font.family: AppMetrics.fontFamily
                        color: AppTheme.colourMuted
                    }

                    TextEdit
                    {
                        objectName: "book_page_location"

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
                    }
                }
            }

            ColumnLayout
            {
                Layout.fillWidth: true
                visible: root.tab === 1
                spacing: 0

                Repeater
                {
                    model: root.tab === 1 ? root.chapters : []

                    delegate: Rectangle
                    {
                        id: chapterRow

                        required property var modelData
                        required property int index

                        readonly property bool current: index + 1 === root.details.chapter

                        objectName: "book_page_chapter_" + index

                        Layout.fillWidth: true
                        implicitHeight: Math.max(AppMetrics.buttonHeightLarge, chapterLabel.implicitHeight + AppMetrics.gapMedium * 2)
                        radius: AppMetrics.radiusSmall
                        color: chapterTap.pressed ? AppTheme.colourFaint : chapterRow.current ? AppTheme.colourSunken : "transparent"

                        Accessible.role: Accessible.Button
                        Accessible.name: modelData.title

                        Text
                        {
                            id: chapterLabel

                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: AppMetrics.gapCompact + chapterRow.modelData.level * AppMetrics.gapMedium
                            anchors.rightMargin: AppMetrics.gapCompact
                            text: chapterRow.modelData.title
                            font.pixelSize: AppMetrics.fontBody
                            font.weight: chapterRow.current ? Font.DemiBold : Font.Normal
                            font.family: AppMetrics.fontFamily
                            color: chapterRow.current ? AppTheme.colourText : AppTheme.colourSubtle
                            wrapMode: Text.Wrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }

                        TapHandler
                        {
                            id: chapterTap

                            onTapped: root.chapterRequested(root.book, chapterRow.modelData.position)
                        }
                    }
                }

                Text
                {
                    Layout.fillWidth: true
                    Layout.topMargin: AppMetrics.gapMedium
                    visible: root.chapters.length === 0 && !root.loading
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("This book has no table of contents.")
                    font.pixelSize: AppMetrics.fontBody
                    font.family: AppMetrics.fontFamily
                    color: AppTheme.colourMuted
                    wrapMode: Text.WordWrap
                }
            }

            ColumnLayout
            {
                Layout.fillWidth: true
                Layout.leftMargin: -AppMetrics.contentPadding
                Layout.rightMargin: -AppMetrics.contentPadding
                visible: root.tab === 2
                spacing: 0

                Repeater
                {
                    model: root.words

                    delegate: MobileWordRow
                    {
                        required property var modelData

                        objectName: "book_page_word"

                        word: modelData.word
                        translation: modelData.translation
                        stage: modelData.stage
                        completed: modelData.completed
                    }
                }

                Text
                {
                    Layout.fillWidth: true
                    Layout.margins: AppMetrics.contentPadding
                    visible: root.words.length === 0
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("Words you add while reading this book will appear here.")
                    font.pixelSize: AppMetrics.fontBody
                    font.family: AppMetrics.fontFamily
                    color: AppTheme.colourMuted
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
}
