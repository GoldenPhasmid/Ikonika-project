import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Shapes

Rectangle
{
    id: root

    property var deck: ({})
    property bool builtin: false
    property bool compact: false
    property bool editing: false
    property bool sheetActions: false
    property string categoryName: ""
    property color tint: AppTheme.colourCoverMuted

    signal learnRequested()
    signal practiceRequested()
    signal importRequested()
    signal exportRequested()
    signal editRequested()
    signal deleteRequested()
    signal categoryChosen(string categoryId)
    signal dailyLearningChosen(bool dailyLearning)

    readonly property int wordCount: root.deck.wordCount ?? 0
    readonly property int newCount: (root.builtin ? root.deck.untriagedCount : root.deck.newCount) ?? 0
    readonly property bool learnable: root.newCount > 0
    readonly property string description: root.deck.description ?? ""
    readonly property string wordsText: qsTr("%n words", "", root.wordCount)
    readonly property real wordCountSafe: Math.max(root.wordCount, 1)
    readonly property color inkColour: AppTheme.colourCoverInk
    readonly property bool narrow: !root.builtin && root.width <= AppMetrics.deckHeaderStackWidth
    readonly property int framePadding: root.narrow ? AppMetrics.gapMedium : AppMetrics.contentPadding

    implicitHeight: column.implicitHeight + AppMetrics.gapMedium * 2 + (root.builtin ? AppMetrics.coverProgressHeight : 0)

    color: root.tint
    radius: root.builtin ? 0 : AppMetrics.deckHeaderRadius
    clip: true

    Accessible.role: Accessible.Grouping
    Accessible.name: root.deck.name ?? ""

    component HeaderGlyph: MobileVectorIcon
    {
        property real glyphSize: AppMetrics.deckHeaderActionIconSize

        implicitWidth: glyphSize
        implicitHeight: glyphSize
        width: glyphSize
        height: glyphSize
        Layout.preferredWidth: glyphSize
        Layout.preferredHeight: glyphSize
        Layout.maximumWidth: glyphSize
        Layout.maximumHeight: glyphSize
        preferredRendererType: Shape.CurveRenderer
        color: root.inkColour
        strokeWidth: AppMetrics.deckHeaderIconStrokeWidth
    }

    component DeckTool: DesktopIconButton
    {
        id: tool

        required property string glyphPath

        onCover: true

        contentItem: Item
        {
            HeaderGlyph
            {
                anchors.centerIn: parent
                svgPath: tool.glyphPath
                color: tool.iconColour
            }
        }
    }

    component DeckAction: DesktopButton
    {
        id: action

        required property string glyphPath

        large: true
        onCover: true
        hairlineOutline: true
        implicitWidth: Math.max(AppMetrics.deckHeaderActionWidth, contentItem.implicitWidth + leftPadding + rightPadding)

        contentItem: Item
        {
            implicitWidth: actionContent.implicitWidth
            implicitHeight: actionContent.implicitHeight

            RowLayout
            {
                id: actionContent

                anchors.centerIn: parent
                width: Math.min(parent.width, implicitWidth)
                spacing: AppMetrics.gapSmall

                HeaderGlyph
                {
                    svgPath: action.glyphPath
                    color: action.contentColour
                }

                DesktopLabel
                {
                    Layout.fillWidth: true
                    text: action.text
                    font.weight: Font.DemiBold
                    color: action.contentColour
                    elide: Text.ElideRight
                }
            }
        }
    }

    Canvas
    {
        anchors.fill: parent
        visible: !root.builtin

        readonly property color stripeColour: AppTheme.colourCoverStripe

        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onStripeColourChanged: requestPaint()

        onPaint:
        {
            const painter = getContext("2d")
            const stripeWidth = AppMetrics.deckStripeWidth
            const stripeHeight = stripeWidth / AppMetrics.readerTileAspect
            const stripeLeft = width - stripeWidth
            const stripeTop = height - stripeHeight

            painter.reset()
            painter.clearRect(0, 0, width, height)
            painter.beginPath()
            painter.roundedRect(0, 0, width, height, root.radius, root.radius)
            painter.clip()
            painter.beginPath()
            painter.moveTo(stripeLeft + stripeWidth * AppMetrics.coverStripeBottomStart, height)
            painter.lineTo(stripeLeft + stripeWidth * AppMetrics.coverStripeBottomEnd, height)
            painter.lineTo(width, stripeTop + stripeHeight * AppMetrics.coverStripeRightEnd)
            painter.lineTo(width, stripeTop + stripeHeight * AppMetrics.coverStripeRightStart)
            painter.closePath()
            painter.fillStyle = stripeColour
            painter.fill()
        }
    }

    Item
    {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        visible: root.builtin
        width: AppMetrics.deckStripeWidth
        height: width / AppMetrics.readerTileAspect

        CoverStripe {}
    }

    ColumnLayout
    {
        id: column

        x: (root.width - width) / 2
        y: AppMetrics.gapMedium
        width: root.builtin ? Math.min(AppMetrics.contentWidthNarrow, root.width - root.framePadding * 2)
                            : Math.min(root.width, AppMetrics.deckHeaderFrameWidth) - root.framePadding * 2
        spacing: root.framePadding

        GridLayout
        {
            Layout.fillWidth: true
            columns: root.builtin ? 4 : (root.narrow ? 2 : 3)
            columnSpacing: AppMetrics.gapMedium
            rowSpacing: AppMetrics.gapTight

            Rectangle
            {
                Layout.alignment: root.builtin ? Qt.AlignVCenter : Qt.AlignTop
                implicitWidth: !root.builtin ? AppMetrics.deckHeaderSymbolSize
                    : (root.compact ? AppMetrics.deckBadgeSizeCompact : AppMetrics.deckBadgeSize)
                implicitHeight: implicitWidth
                radius: AppMetrics.coverRadius
                color: root.builtin ? AppTheme.colourCoverStripe : "transparent"

                DesktopIcon
                {
                    anchors.centerIn: parent
                    width: root.builtin ? AppMetrics.deckIconSize : AppMetrics.deckHeaderSymbolSize
                    height: width
                    name: root.deck.icon || "nav_decks"
                    color: root.inkColour
                }
            }

            ColumnLayout
            {
                Layout.alignment: root.builtin ? Qt.AlignVCenter : Qt.AlignTop
                Layout.fillWidth: true
                spacing: AppMetrics.gapTight

                DesktopLabel
                {
                    id: nameLabel

                    Layout.fillWidth: true
                    Layout.preferredHeight: root.builtin ? implicitHeight : lineCount * lineHeight
                    text: root.deck.name ?? ""
                    font.pixelSize: (root.builtin ? root.compact : root.narrow) ? AppMetrics.fontLead : AppMetrics.fontDeckName
                    font.weight: Font.DemiBold
                    color: root.inkColour
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    lineHeightMode: root.builtin ? Text.ProportionalHeight : Text.FixedHeight
                    lineHeight: root.builtin ? 1 : font.pixelSize * AppMetrics.deckHeaderTitleLeading

                    Accessible.role: Accessible.Heading
                    Accessible.name: nameLabel.text
                }

                DesktopLabel
                {
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.builtin ? implicitHeight : Math.floor(lineCount * lineHeight)
                    visible: root.description.length > 0
                    text: root.description
                    color: root.inkColour
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    lineHeightMode: root.builtin ? Text.ProportionalHeight : Text.FixedHeight
                    lineHeight: root.builtin ? 1 : font.pixelSize * AppMetrics.deckHeaderDescriptionLeading
                }

                AbstractButton
                {
                    id: describeButton

                    objectName: "deck_describe"

                    visible: !root.builtin && root.description.length === 0
                    implicitHeight: contentItem.implicitHeight
                    implicitWidth: contentItem.implicitWidth
                    padding: 0
                    hoverEnabled: true
                    focusPolicy: Qt.StrongFocus
                    text: qsTr("Add a description")

                    Accessible.name: describeButton.text

                    onClicked: root.editRequested()

                    contentItem: DesktopLabel
                    {
                        text: describeButton.text
                        font.underline: true
                        color: root.inkColour
                        opacity: describeButton.hovered ? AppTheme.opacityHover : 1.0
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Item
                    {
                        DesktopFocusRing
                        {
                            visible: describeButton.visualFocus
                            colour: root.inkColour
                        }
                    }

                    HoverHandler
                    {
                        cursorShape: Qt.PointingHandCursor
                    }
                }

                DesktopLabel
                {
                    Layout.fillWidth: true
                    visible: root.builtin
                    text: root.categoryName.length > 0 ? root.categoryName + " · " + root.wordsText : root.wordsText
                    font.pixelSize: AppMetrics.fontSmall
                    color: root.inkColour
                    elide: Text.ElideRight
                }
            }

            Row
            {
                Layout.row: root.narrow ? 1 : 0
                Layout.column: root.narrow ? 1 : 2
                Layout.alignment: Qt.AlignTop
                visible: !root.builtin && !root.sheetActions
                spacing: AppMetrics.gapTight

                DeckTool
                {
                    objectName: "deck_edit"
                    text: qsTr("Edit deck")
                    glyphPath: "M4 16l-1 5 5-1L20 8l-4-4z M13 7l4 4"

                    onClicked: root.editRequested()
                }

                DeckTool
                {
                    objectName: "deck_import_into"
                    text: qsTr("Import")
                    glyphPath: "M12 3v12m-5-5 5 5 5-5 M4 16v5h16v-5"

                    onClicked: root.importRequested()
                }

                DeckTool
                {
                    objectName: "deck_export"
                    text: qsTr("Export")
                    glyphPath: "M12 16V3m-5 5 5-5 5 5 M4 16v5h16v-5"

                    onClicked: root.exportRequested()
                }

                DeckTool
                {
                    objectName: "deck_delete"
                    text: qsTr("Delete deck")
                    destructive: true
                    glyphPath: "M3 6h18 M9 6V3h6v3 M6 6l1 15h10l1-15 M10 10v7m4-7v7"

                    onClicked: root.deleteRequested()
                }
            }

            DesktopIconButton
            {
                objectName: "deck_export"
                visible: root.builtin && !root.sheetActions
                onCover: true
                iconName: "upload"
                text: qsTr("Export")

                onClicked: root.exportRequested()
            }

            DesktopButton
            {
                objectName: "deck_learn"
                visible: root.builtin
                large: true
                onCover: true
                tone: root.editing ? "secondary" : "primary"
                enabled: root.learnable
                text: root.learnable ? qsTr("Discover %1").arg(root.newCount) : qsTr("Discover")
                keyHint: root.learnable && !root.editing && !root.compact ? qsTr("Enter") : ""

                onClicked: root.learnRequested()
            }
        }

        GridLayout
        {
            id: controls

            readonly property bool stacked: root.narrow
                || width < trainingActions.implicitWidth + (preferences.visible ? preferences.implicitWidth + columnSpacing : 0)

            Layout.fillWidth: true
            visible: !root.builtin
            columns: controls.stacked ? 1 : 2
            columnSpacing: AppMetrics.gapCompact
            rowSpacing: AppMetrics.gapSmall

            RowLayout
            {
                id: trainingActions

                Layout.fillWidth: controls.stacked
                spacing: AppMetrics.gapSmall

                DeckAction
                {
                    objectName: "deck_learn"
                    Layout.fillWidth: controls.stacked
                    Layout.minimumWidth: implicitHeight
                    tone: root.editing ? "secondary" : "primary"
                    enabled: root.learnable
                    text: root.learnable ? qsTr("Learn %1").arg(root.newCount) : qsTr("Learn")
                    glyphPath: "M4 5q4-2 8 1 4-3 8-1v14q-4-2-8 1-4-3-8-1z M12 6v14"

                    onClicked: root.learnRequested()
                }

                DeckAction
                {
                    objectName: "deck_practice"
                    Layout.fillWidth: controls.stacked
                    Layout.minimumWidth: implicitHeight
                    enabled: root.wordCount > 0
                    text: qsTr("Practice")
                    glyphPath: "M4 10a8 8 0 1 1 1 7 M4 4v6h6"

                    onClicked: root.practiceRequested()
                }
            }

            RowLayout
            {
                id: preferences

                Layout.fillWidth: true
                visible: !root.sheetActions
                spacing: AppMetrics.gapSmall

                Item { Layout.fillWidth: true; visible: !controls.stacked }

                Button
                {
                    id: categoryChip

                    objectName: "deck_category"

                    Layout.fillWidth: true
                    Layout.maximumWidth: implicitWidth
                    implicitWidth: Math.min(AppMetrics.popoverWidth, categoryContent.implicitWidth + leftPadding + rightPadding)
                    implicitHeight: AppMetrics.deckHeaderTagHeight
                    horizontalPadding: AppMetrics.gapSmall
                    verticalPadding: 0
                    hoverEnabled: true
                    focusPolicy: Qt.StrongFocus
                    text: root.categoryName.length > 0 ? root.categoryName : qsTr("No category")

                    Accessible.name: qsTr("Category: %1").arg(categoryChip.text)

                    onClicked: categoryPopover.visible ? categoryPopover.close() : categoryPopover.open()

                    contentItem: RowLayout
                    {
                        id: categoryContent

                        spacing: AppMetrics.gapSmall

                        HeaderGlyph
                        {
                            glyphSize: AppMetrics.deckHeaderTagIconSize
                            svgPath: "M3 3h9l9 9-9 9-9-9z M7 7h.1"
                        }

                        DesktopLabel
                        {
                            Layout.fillWidth: true
                            text: categoryChip.text
                            font.pixelSize: AppMetrics.fontSmall
                            color: root.inkColour
                            elide: Text.ElideRight
                        }

                        HeaderGlyph
                        {
                            glyphSize: AppMetrics.deckHeaderTagIconSize
                            svgPath: "m6 9 6 6 6-6"
                        }
                    }

                    background: Rectangle
                    {
                        radius: height / 2
                        color: Qt.tint(AppTheme.colourDeckHeaderTag, AppTheme.faded(root.inkColour,
                                      categoryChip.down ? AppTheme.alphaSoft : (categoryChip.hovered ? AppTheme.alphaFaint : 0)))

                        DesktopFocusRing
                        {
                            visible: categoryChip.visualFocus
                            baseRadius: parent.radius
                            colour: root.inkColour
                        }
                    }

                    HoverHandler
                    {
                        cursorShape: Qt.PointingHandCursor
                    }

                    DesktopPopover
                    {
                        id: categoryPopover

                        y: categoryChip.height + AppMetrics.gapTight
                        heading: qsTr("Category")

                        Repeater
                        {
                            model: [{ id: "", name: qsTr("No category") }].concat(UserDecksController.categories)

                            delegate: DesktopPopoverRow
                            {
                                required property var modelData

                                objectName: "deck_category_" + modelData.id
                                text: modelData.name
                                showsCheck: true
                                selected: (root.deck.categoryId ?? "") === modelData.id

                                onClicked:
                                {
                                    categoryPopover.close()
                                    root.categoryChosen(modelData.id)
                                }
                            }
                        }
                    }
                }

                Item { Layout.fillWidth: true; visible: controls.stacked }

                CheckBox
                {
                    id: dailyLearning

                    objectName: "deck_daily_learning"
                    implicitHeight: AppMetrics.deckHeaderTagHeight
                    implicitWidth: leftPadding + contentItem.implicitWidth
                    padding: 0
                    leftPadding: AppMetrics.gapSmall + indicator.width + spacing
                    spacing: AppMetrics.gapSmall
                    hoverEnabled: true
                    focusPolicy: Qt.StrongFocus
                    text: qsTr("Daily learning")
                    checked: root.deck.dailyLearning ?? true

                    Accessible.name: dailyLearning.text

                    onClicked: root.dailyLearningChosen(checked)

                    indicator: Rectangle
                    {
                        x: AppMetrics.gapSmall
                        anchors.verticalCenter: parent.verticalCenter
                        width: AppMetrics.deckHeaderTagIconSize
                        height: width
                        radius: AppMetrics.deckHeaderCheckRadius
                        color: dailyLearning.checked ? root.inkColour : "transparent"
                        border.width: AppMetrics.borderWidth
                        border.color: root.inkColour

                        DesktopIcon
                        {
                            anchors.fill: parent
                            visible: dailyLearning.checked
                            filled: true
                            color: AppTheme.colourDeckHeaderCheck
                            svgPath: "M3 13l3-3 3 3 8-9 3 2.5L9 20z"
                        }
                    }

                    contentItem: DesktopLabel
                    {
                        text: dailyLearning.text
                        font.pixelSize: AppMetrics.fontSmall
                        color: root.inkColour
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle
                    {
                        radius: AppMetrics.radiusMedium
                        color: AppTheme.faded(root.inkColour,
                                             dailyLearning.down ? AppTheme.alphaSoft : (dailyLearning.hovered ? AppTheme.alphaFaint : 0))

                        DesktopFocusRing
                        {
                            visible: dailyLearning.visualFocus
                            colour: root.inkColour
                        }
                    }

                    HoverHandler
                    {
                        cursorShape: Qt.PointingHandCursor
                    }
                }
            }
        }
    }

    Rectangle
    {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        visible: root.builtin
        height: AppMetrics.coverProgressHeight
        color: AppTheme.faded(root.inkColour, AppTheme.alphaStrong)

        Row
        {
            anchors.fill: parent

            Rectangle
            {
                width: parent.width * (root.deck.learnedCount ?? 0) / root.wordCountSafe
                height: parent.height
                color: root.inkColour
            }

            Rectangle
            {
                width: parent.width * (root.deck.learningCount ?? 0) / root.wordCountSafe
                height: parent.height
                color: AppTheme.faded(root.inkColour, AppTheme.alphaHalf)
            }
        }
    }
}
