import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../ReaderShelf.js" as Shelf

Item
{
    id: root

    required property DailySessionController daily

    required property var store
    property var training: null
    property var review: null
    property var sentences: null
    property var discovery: null

    signal learnRequested()
    signal reviewRequested()
    signal formsTrainingRequested(string training)
    signal sentencesRequested()
    signal practiceRequested()
    signal discoverRequested()
    signal wordListRequested(string source)
    signal messageRequested(string message)
    signal bookRequested(string documentId)
    signal freeBookRequested(string freeId)
    signal openFileRequested()

    property bool hasLearnSession: false
    property var books: []
    property var freeBooks: []
    property var freeTransfers: ({})

    readonly property bool continuing: root.books.length > 0
    readonly property var startBooks: root.continuing ? [] : Shelf.startBooks(root.freeBooks, root.vocabularyTotal).slice(0, 2)

    function startFreeBook(book)
    {
        if (book.local)
        {
            root.bookRequested(book.id)

            return
        }

        if (root.freeTransfers[book.freeId] !== undefined)
        {
            return
        }

        console.info("Today: downloading free book", book.freeId)
        root.freeBookRequested(book.freeId)
    }
    readonly property bool hasReviewSession: root.review ? root.review.hasActiveSession : false
    readonly property bool hasDiscoverSession: root.discovery ? root.discovery.hasActiveSession : false

    readonly property int wordsLeftForGoal: root.daily.wordsLeftForGoal

    readonly property int reviewCount: root.hasReviewSession ? root.review.cardCount : root.daily.dueCount

    readonly property bool sentencesReady: !!root.sentences
        && (root.sentences.savedExercises.length > 0 || root.sentences.unlocked)

    readonly property Item goalRing: ring
    readonly property int dailyGoal: root.daily.dailyGoal
    readonly property int doneToday: root.daily.learnedToday
    readonly property real goalRatio: root.daily.dailyGoal > 0 ? Math.min(root.doneToday / root.daily.dailyGoal, 1.0) : 0.0

    readonly property bool reviewDone: !root.hasReviewSession && root.daily.dueCount === 0
    readonly property bool hasLearnGoal: root.dailyGoal > 0
    readonly property bool learnDone: !root.hasLearnGoal || root.goalRatio >= 1
    readonly property int reviewTotal: root.daily.reviewedToday + root.daily.dueCount
    readonly property real reviewRatio: root.reviewTotal > 0 ? root.daily.reviewedToday / root.reviewTotal : 1.0
    readonly property bool hasReviewGoal: root.reviewTotal > 0
    readonly property int ringDone: root.hasLearnGoal ? root.doneToday : root.daily.reviewedToday
    readonly property int ringGoal: root.hasLearnGoal ? root.dailyGoal : root.reviewTotal

    readonly property var comingDays: root.daily.reviewTimeline.filter(Day => Day.daysFromToday > 0).slice(0, 4)

    readonly property int vocabularyTotal: root.daily.ladder.reduce((Sum, Entry) => Sum + Entry.count, 0)

    function weekdayIndex()
    {
        return root.daily.weekday - Qt.Monday
    }

    readonly property int learnedThisWeek:
    {
        const todayIndex = root.weekdayIndex()

        return root.daily.recentProgress.filter(Entry => Entry.daysAgo <= todayIndex)
                                   .reduce((Sum, Entry) => Sum + Entry.learnedCount, 0)
    }

    readonly property var weekDays:
    {
        const locale = Qt.locale(AppLocalization.localeName)
        const todayIndex = root.weekdayIndex()
        const days = []

        for (let index = 0; index < 7; ++index)
        {
            const today = index === todayIndex
            const recorded = root.daily.recentProgress.find(Entry => Entry.daysAgo === todayIndex - index)

            days.push({
                name: locale.dayName((Qt.Monday + index) % 7, Locale.ShortFormat),
                today: today,
                past: index < todayIndex,
                review: today ? root.reviewRatio : (recorded ? recorded.review : 0),
                learn: today ? root.goalRatio : (recorded ? recorded.learn : 0)
            })
        }

        return days
    }

    property var formsTrainings: []
    property var formsTrainingKeys: []

    readonly property var formsTrainingKinds: [
        { key: "conjugation", icon: "session_conjugation", label: qsTr("Conjugations"),
          lockedCaption: qsTr("Learn more verbs"),
          blockedReason: qsTr("Learn more verbs to unlock conjugations") },
        { key: "forms", icon: "session_forms", label: qsTr("Forms"),
          lockedCaption: qsTr("Learn more nouns"),
          blockedReason: qsTr("Learn more irregular nouns and adjectives to unlock forms") }
    ]

    function listFormsTrainings()
    {
        const offered = []

        for (const kind of root.formsTrainingKinds)
        {
            const trainingId = root.daily.formsTrainingId(kind.key)

            if (trainingId.length === 0)
            {
                continue
            }

            const resumable = root.training ? root.training.hasSessionFor(trainingId) : false

            offered.push({
                key: kind.key,
                icon: kind.icon,
                label: kind.label,
                resumable: resumable,
                lockedCaption: kind.lockedCaption,
                blockedReason: kind.blockedReason,
                wordCount: root.daily.formsWordCount(trainingId),
                ready: resumable || root.daily.canStartFormsTraining(trainingId)
            })
        }

        return offered
    }

    function applyFormsTrainings(offered)
    {
        const keys = offered.map(Training => Training.key)

        root.formsTrainings = offered

        if (JSON.stringify(keys) !== JSON.stringify(root.formsTrainingKeys))
        {
            root.formsTrainingKeys = keys
        }
    }

    function refresh()
    {
        root.daily.refresh()
        root.hasLearnSession = root.training ? root.training.hasSessionFor(root.training.vocabularyKey) : false
        root.applyFormsTrainings(root.listFormsTrainings())

        if (root.sentences)
        {
            root.sentences.refresh()
        }
    }

    function sessionTile(key)
    {
        switch (key)
        {
        case "review": return reviewTile
        case "learn": return learnTile
        case "sentences": return sentencesTile
        case "discover": return discoverTile
        }

        for (let index = 0; index < formsTiles.count; ++index)
        {
            const tile = formsTiles.itemAt(index)

            if (tile && tile.modelData === key)
            {
                return tile
            }
        }

        return null
    }

    function formsCaption(training)
    {
        if (training.resumable)
        {
            return qsTr("Continue")
        }

        if (!training.ready)
        {
            return training.lockedCaption
        }

        return training.key === "conjugation" ? qsTr("%n verbs", "", training.wordCount)
                                              : qsTr("%n words", "", training.wordCount)
    }

    function formsCount(training)
    {
        if (training.resumable)
        {
            return qsTr("Continue")
        }

        return training.ready ? training.wordCount.toString() : ""
    }

    function sentencesCount()
    {
        if (!root.sentences)
        {
            return ""
        }

        if (root.sentences.savedExercises.length > 0)
        {
            return qsTr("Continue")
        }

        return root.sentencesReady ? root.sentences.readyCount.toString() : ""
    }

    function sentencesCaption()
    {
        if (!root.sentences)
        {
            return ""
        }

        if (root.sentences.savedExercises.length > 0)
        {
            return qsTr("Continue")
        }

        if (!root.sentencesReady)
        {
            return root.sentences.available || root.sentences.refreshing ? qsTr("Learn more words") : qsTr("No sentences yet")
        }

        return qsTr("%L1 ready").arg(root.sentences.readyCount)
    }

    function dayLabel(daysFromToday)
    {
        if (daysFromToday === 0)
        {
            return qsTr("Today")
        }

        if (daysFromToday === 1)
        {
            return qsTr("Tomorrow")
        }

        const date = new Date(root.daily.learningDate)
        date.setDate(date.getDate() + daysFromToday)

        return Qt.locale(AppLocalization.localeName).toString(date, "dddd")
    }

    function shortDayLabel(daysFromToday)
    {
        const date = new Date(root.daily.learningDate)
        date.setDate(date.getDate() + daysFromToday)

        return Qt.locale(AppLocalization.localeName).toString(date, "ddd")
    }

    onVisibleChanged:
    {
        if (!root.visible)
        {
            return
        }

        root.refresh()
        root.celebrateGoal()
    }
    Component.onCompleted: if (root.visible) root.refresh()

    Connections
    {
        target: root.training
        enabled: root.visible

        function onStateChanged()
        {
            root.hasLearnSession = root.training.hasSessionFor(root.training.vocabularyKey)
            root.applyFormsTrainings(root.listFormsTrainings())
        }
    }

    onGoalRatioChanged: root.celebrateGoal()

    function celebrateGoal()
    {
        if (root.goalRatio < 1 || !root.visible)
        {
            return
        }

        AppSoundEffects.celebrateDailyGoal()
    }

    Connections
    {
        target: root.daily

        function onFormsCountsChanged() { root.applyFormsTrainings(root.listFormsTrainings()) }
    }

    Rectangle
    {
        anchors.fill: parent
        color: AppTheme.colourSunken
    }

    Item
    {
        id: area

        readonly property real zoom: Math.min(area.width / AppMetrics.windowMinimumWidth,
                                              area.height / (AppMetrics.windowMinimumHeight - AppMetrics.topBarHeight),
                                              AppMetrics.todayMaximumZoom)

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: notices.visible ? notices.top : parent.bottom

        Item
        {
            x: (area.width - width * area.zoom) / 2
            y: (area.height - height * area.zoom) / 2
            width: area.zoom > 0 ? Math.min(area.width / area.zoom, AppMetrics.todayMaximumWidth) : 0
            height: area.zoom > 0 ? Math.min(area.height / area.zoom, AppMetrics.todayMaximumHeight) : 0
            scale: area.zoom
            transformOrigin: Item.TopLeft

            Item
            {
                id: board

                readonly property real middleWidth: Math.min(AppMetrics.todayPrimaryWidth, board.width - AppMetrics.todayColumnGap * 2)
                readonly property real sideWidth: (board.width - board.middleWidth - AppMetrics.todayColumnGap * 2) / 2
                readonly property real middleLeft: board.sideWidth + AppMetrics.todayColumnGap
                readonly property real rightLeft: board.middleLeft + board.middleWidth + AppMetrics.todayColumnGap
                readonly property real stripHeight: AppMetrics.todayLabelHeight * 2 + AppMetrics.todayTileGap
                                                    + AppMetrics.todayNameGap + AppMetrics.weekRingSize
                readonly property real stripTop: ring.height / 2 - (board.stripHeight - AppMetrics.weekRingSize / 2)
                readonly property real gridTop: board.stripHeight + AppMetrics.gapLarge
                readonly property real bookWidth: (board.width - board.middleLeft - AppMetrics.todayTileGap) / 2
                readonly property real readingHeight: AppMetrics.todayLabelHeight + AppMetrics.todayTileGap
                                                      + board.bookWidth / AppMetrics.todayBookAspect
                readonly property real upperHeight: board.height - board.readingHeight - AppMetrics.gapLarge - board.gridTop
                readonly property real catRoom: board.height - ring.height - AppMetrics.gapLarge
                readonly property real busiestDay: Math.max(1, ...root.comingDays.map(Day => Day.count))

                anchors.fill: parent
                anchors.leftMargin: AppMetrics.todayInsetX
                anchors.rightMargin: AppMetrics.todayInsetX
                anchors.topMargin: AppMetrics.todayInsetY
                anchors.bottomMargin: AppMetrics.todayInsetY

                Image
                {
                    objectName: "today_cat"

                    width: Math.min(board.sideWidth, board.catRoom * AppMetrics.todayCatAspect)
                    height: width / AppMetrics.todayCatAspect
                    y: board.height - height
                    source: "qrc:/images/LufiExcited.svg"
                    sourceSize: Qt.size(width, height)
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                }

                DesktopDailyRing
                {
                    id: ring

                    outerProgress: root.reviewRatio
                    innerProgress: root.goalRatio
                    showOuter: root.hasReviewGoal
                    showInner: root.hasLearnGoal
                    doneToday: root.ringDone
                    dailyGoal: root.ringGoal
                }

                Rectangle
                {
                    id: link

                    readonly property real reach: AppMetrics.todayLinkGap + comingRow.width + AppMetrics.todayLinkReach

                    x: ring.width
                    y: ring.height / 2
                    width: link.reach
                    height: AppMetrics.borderWidth

                    gradient: Gradient
                    {
                        orientation: Gradient.Horizontal

                        GradientStop { position: 0; color: AppTheme.colourOutline }
                        GradientStop { position: AppMetrics.todayLinkGlow / link.reach; color: AppTheme.colourBorder }
                        GradientStop { position: 1 - AppMetrics.todayLinkReach / link.reach; color: AppTheme.colourBorder }
                        GradientStop { position: 1; color: AppTheme.faded(AppTheme.colourBorder, 0) }
                    }
                }

                ColumnLayout
                {
                    objectName: "today_coming_up"

                    x: ring.width + AppMetrics.todayLinkGap
                    y: board.stripTop
                    spacing: AppMetrics.todayTileGap

                    Item
                    {
                        Layout.fillWidth: true
                        Layout.preferredHeight: AppMetrics.todayLabelHeight

                        DesktopRuledLabel
                        {
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width
                            text: qsTr("Coming up")
                        }
                    }

                    Row
                    {
                        id: comingRow

                        spacing: AppMetrics.todayDayGap

                        Repeater
                        {
                            model: root.comingDays

                            delegate: DesktopDueDay
                            {
                                required property var modelData

                                width: AppMetrics.weekRingSize

                                name: root.shortDayLabel(modelData.daysFromToday)
                                dayName: root.dayLabel(modelData.daysFromToday)
                                count: modelData.count
                                heat: modelData.count / board.busiestDay

                                onClicked: root.wordListRequested("due:" + modelData.daysFromToday)
                            }
                        }
                    }
                }

                ColumnLayout
                {
                    anchors.right: parent.right
                    y: board.stripTop
                    spacing: AppMetrics.todayTileGap

                    Item
                    {
                        Layout.fillWidth: true
                        Layout.preferredHeight: AppMetrics.todayLabelHeight

                        DesktopRuledLabel
                        {
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width
                            text: qsTr("This week")
                            trailing: root.learnedThisWeek.toString()
                        }
                    }

                    Row
                    {
                        spacing: AppMetrics.todayDayGap

                        Repeater
                        {
                            model: root.weekDays

                            delegate: DesktopWeekDay
                            {
                                required property var modelData

                                width: AppMetrics.weekRingSize

                                name: modelData.name
                                today: modelData.today
                                past: modelData.past
                                review: modelData.review
                                learn: modelData.learn
                            }
                        }
                    }
                }

                ColumnLayout
                {
                    x: board.middleLeft
                    y: board.gridTop
                    width: board.middleWidth
                    height: board.upperHeight
                    spacing: AppMetrics.todayTileGap

                    DesktopSessionTile
                    {
                        id: reviewTile

                        objectName: "today_review"

                        Layout.fillHeight: true

                        primary: true
                        iconName: "session_review"
                        dotColour: AppTheme.colourAccent
                        text: root.hasReviewSession ? qsTr("Continue") : qsTr("Review")
                        caption: root.reviewDone ? qsTr("Done for today") : ""
                        count: root.daily.dueCount.toString()
                        completed: root.reviewDone
                        blocked: root.reviewDone

                        onActivated: root.reviewRequested()
                        onBlockedClicked: root.messageRequested(root.daily.reviewedToday > 0 ? qsTr("All reviewed for today")
                                                                                        : qsTr("Nothing is due for review yet"))
                    }

                    DesktopSessionTile
                    {
                        id: learnTile

                        objectName: "today_learn"

                        Layout.fillHeight: true

                        primary: root.hasLearnSession || (root.reviewDone && !root.learnDone)
                        iconName: "session_learn"
                        dotColour: AppTheme.colourCorrect
                        text: root.hasLearnSession ? qsTr("Continue") : qsTr("Learn new words")
                        caption: !root.hasLearnGoal ? ""
                                 : root.learnDone ? qsTr("Goal reached")
                                 : (root.hasLearnSession ? qsTr("%1 of %2 learned").arg(root.doneToday).arg(root.dailyGoal) : "")
                        count: root.hasLearnSession ? root.training.wordCountFor(root.training.vocabularyKey).toString()
                                                   : (root.hasLearnGoal ? root.daily.wordsLeftForGoal.toString() : "")
                        completed: !root.hasLearnSession && root.hasLearnGoal && root.learnDone

                        onActivated: root.learnRequested()
                    }
                }

                ColumnLayout
                {
                    id: sessions

                    readonly property int rows: 3 + root.formsTrainings.length
                    readonly property real unit: (sessions.height - sessions.spacing * (sessions.rows - 1) - AppMetrics.gapSmall * 2)
                                                 / (sessions.rows + 2)

                    width: Math.min(AppMetrics.todaySessionsWidth, board.sideWidth)
                    x: board.rightLeft + board.sideWidth - width
                    y: board.gridTop
                    height: board.upperHeight
                    spacing: AppMetrics.todaySessionGap

                    DesktopSessionTile
                    {
                        id: discoverTile

                        objectName: "today_discover"

                        Layout.preferredHeight: sessions.unit * 3
                        Layout.bottomMargin: AppMetrics.gapSmall

                        primary: root.hasDiscoverSession
                        iconName: "nav_decks"
                        text: qsTr("Discover")
                        caption: !DecksController.categorised ? DecksController.discoverBlockedReason
                               : root.hasDiscoverSession ? qsTr("Continue") : qsTr("Find new words")
                        blocked: !DecksController.categorised

                        onActivated: root.discoverRequested()
                        onBlockedClicked: root.messageRequested(DecksController.discoverBlockedReason)
                    }

                    DesktopSessionTile
                    {
                        id: sentencesTile

                        objectName: "today_sentences"

                        Layout.preferredHeight: sessions.unit
                        Layout.bottomMargin: root.formsTrainings.length > 0 ? 0 : AppMetrics.gapSmall

                        compact: true
                        primary: !!root.sentences && root.sentences.savedExercises.length > 0
                        iconName: "session_sentences"
                        text: qsTr("Sentences")
                        caption: root.sentencesCaption()
                        count: root.sentencesCount()
                        blocked: !root.sentencesReady

                        onActivated: root.sentencesRequested()
                        onBlockedClicked: root.messageRequested(root.sentences && !root.sentences.available
                                                                ? qsTr("This dictionary has no sentences yet")
                                                                : qsTr("Learn more words to unlock sentences"))
                    }

                    Repeater
                    {
                        id: formsTiles

                        model: root.formsTrainingKeys

                        delegate: DesktopSessionTile
                        {
                            required property string modelData
                            required property int index

                            readonly property var training: root.formsTrainings.find(Training => Training.key === modelData) || ({})

                            objectName: "today_" + modelData

                            Layout.preferredHeight: sessions.unit
                            Layout.bottomMargin: index === root.formsTrainings.length - 1 ? AppMetrics.gapSmall : 0

                            compact: true
                            primary: !!training.resumable
                            iconName: training.icon || ""
                            text: training.label || ""
                            caption: root.formsCaption(training)
                            count: root.formsCount(training)
                            blocked: !training.ready

                            onActivated: root.formsTrainingRequested(modelData)
                            onBlockedClicked: root.messageRequested(training.blockedReason)
                        }
                    }

                    DesktopSessionTile
                    {
                        objectName: "today_practice"

                        Layout.preferredHeight: sessions.unit

                        compact: true
                        iconName: "exercise_flashcards"
                        text: qsTr("Practice")
                        caption: root.vocabularyTotal > 0 ? qsTr("%n words", "", root.vocabularyTotal)
                                                          : qsTr("Learn some words")
                        count: root.vocabularyTotal > 0 ? root.vocabularyTotal.toString() : ""
                        blocked: root.vocabularyTotal === 0

                        onActivated: root.practiceRequested()
                        onBlockedClicked: root.messageRequested(qsTr("Learn some words to practise them"))
                    }
                }

                ColumnLayout
                {
                    objectName: "today_continue_reading"

                    x: board.middleLeft
                    y: board.height - board.readingHeight
                    width: board.width - board.middleLeft
                    spacing: AppMetrics.todayTileGap

                    Item
                    {
                        Layout.fillWidth: true
                        Layout.preferredHeight: AppMetrics.todayLabelHeight

                        DesktopRuledLabel
                        {
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width
                            text: root.continuing ? qsTr("Continue reading") : qsTr("Start reading")
                        }
                    }

                    RowLayout
                    {
                        spacing: AppMetrics.todayTileGap

                        Repeater
                        {
                            model: 2

                            delegate: Item
                            {
                                id: bookSlot

                                required property int index

                                readonly property var book: root.continuing && bookSlot.index < root.books.length ? root.books[bookSlot.index] : null
                                readonly property var freeBook: bookSlot.index < root.startBooks.length ? root.startBooks[bookSlot.index] : null
                                readonly property bool opensFile: !root.continuing && bookSlot.freeBook === null
                                readonly property bool wide: bookSlot.opensFile && root.startBooks.length === 0
                                readonly property real downloadProgress:
                                {
                                    if (bookSlot.freeBook === null || bookSlot.freeBook.local)
                                    {
                                        return -1
                                    }

                                    const value = root.freeTransfers[bookSlot.freeBook.freeId]

                                    return value === undefined ? -1 : value
                                }

                                visible: !(bookSlot.index > 0 && !root.continuing && root.startBooks.length === 0)
                                Layout.preferredWidth: bookSlot.wide ? board.bookWidth * 2 + AppMetrics.todayTileGap : board.bookWidth
                                Layout.preferredHeight: board.bookWidth / AppMetrics.todayBookAspect

                                DesktopDashedBox
                                {
                                    anchors.fill: parent
                                    visible: root.continuing && bookSlot.book === null
                                }

                                DesktopContinueBook
                                {
                                    objectName: "today_start_book_" + bookSlot.index

                                    anchors.fill: parent
                                    visible: bookSlot.freeBook !== null
                                    text: bookSlot.freeBook ? bookSlot.freeBook.title : ""
                                    author: bookSlot.freeBook ? bookSlot.freeBook.author || "" : ""
                                    coverSource: bookSlot.freeBook ? bookSlot.freeBook.cover || "" : ""
                                    level: bookSlot.freeBook ? Shelf.levelLabel(bookSlot.freeBook.level) || qsTr("Free book") : ""
                                    needsDownload: bookSlot.freeBook ? !bookSlot.freeBook.local : false
                                    downloadProgress: bookSlot.downloadProgress

                                    onClicked: root.startFreeBook(bookSlot.freeBook)
                                }

                                DesktopContinueBook
                                {
                                    objectName: "today_open_book"

                                    anchors.fill: parent
                                    visible: bookSlot.opensFile
                                    iconName: "folder_plus"
                                    text: qsTr("Open a book")
                                    author: qsTr("PDF, EPUB or FB2 from your computer")

                                    onClicked:
                                    {
                                        console.info("Today: opening a book from the computer")
                                        root.openFileRequested()
                                    }
                                }

                                DesktopContinueBook
                                {
                                    objectName: "today_book_" + bookSlot.index

                                    anchors.fill: parent
                                    visible: bookSlot.book !== null
                                    text: bookSlot.book ? bookSlot.book.title : ""
                                    author: bookSlot.book ? bookSlot.book.author || "" : ""
                                    coverSource: bookSlot.book ? bookSlot.book.cover || "" : ""
                                    progress: bookSlot.book ? bookSlot.book.progress : 0

                                    onClicked: root.bookRequested(bookSlot.book.id)
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    ColumnLayout
    {
        id: notices

        visible: AppSyncController.needsReconnect

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: AppMetrics.gapSmall
        spacing: AppMetrics.gapSmall

        DesktopReconnectBanner
        {
            id: reconnectBanner

            objectName: "today_reconnect_banner"

            Layout.fillWidth: false
            Layout.preferredWidth: Math.min(AppMetrics.contentWidthWide, root.width - AppMetrics.contentPadding * 2)
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: AppMetrics.contentPadding - AppMetrics.gapSmall
        }
    }
}
