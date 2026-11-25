import QtQuick
import QtQuick.Layouts
import Ikonika
import "MobileIcons.js" as Icons
import "../ReaderShelf.js" as Shelf

Item
{
    id: root

    required property DailySessionController daily

    required property var store
    property var repetition: null
    property var intensive: null
    property var sentences: null
    property var discovery: null
    property var reader: null
    property bool lufiActive: root.visible

    readonly property bool sentencesReady: root.sentences
        ? root.sentences.savedExercises.length > 0 || root.sentences.unlocked
        : false

    readonly property string lockedHint: qsTr("Learn more words to unlock")

    readonly property string sentencesBlockedReason: root.sentences && !root.sentences.available && !root.sentences.refreshing
        ? qsTr("This dictionary has no sentences yet")
        : root.lockedHint

    readonly property bool hasReviewSession: root.repetition
        ? root.repetition.hasActiveSession
        : false

    property bool hasLearnSession: false

    function findLearnSession()
    {
        return root.intensive
            ? root.intensive.controller.hasSessionFor(root.intensive.controller.vocabularyKey)
            : false
    }

    signal intensiveRequested()
    signal discoverRequested()
    signal repetitionRequested()
    signal sentencesRequested()
    signal practiceRequested()
    signal dictionaryRequested()
    signal settingsRequested()
    signal wordListRequested(string source)
    signal wordLookupRequested(string term)
    signal tabSelected(string key)
    signal bookRequested(var book)
    signal freeBookRequested(var book)
    signal messageRequested(string message)

    function startFromPool()
    {
        root.intensiveRequested()
    }

    readonly property int vocabularyTotal: root.daily.ladder.reduce((Sum, Entry) => Sum + Entry.count, 0)

    function refresh()
    {
        root.daily.refresh()
        root.hasLearnSession = root.findLearnSession()

        if (root.sentences)
        {
            root.sentences.refresh()
        }
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
        target: Qt.application

        function onStateChanged()
        {
            if (root.visible && Qt.application.state === Qt.ApplicationActive)
                root.daily.refresh()
        }
    }

    Connections
    {
        target: root.intensive ? root.intensive.controller : null
        enabled: root.visible

        function onStateChanged()
        {
            root.hasLearnSession = root.findLearnSession()
        }
    }

    Connections
    {
        target: root.daily

        function onCountsChanged() { root.publishWidget() }
    }

    Connections
    {
        target: AppTheme

        function onThemeChanged()
        {
            root.publishWidget()
        }
    }

    function publishWidget()
    {
        const tomorrow = root.daily.reviewTimeline.find(Entry => Entry.daysFromToday === 1)

        AppHomeWidget.publish(root.doneToday, root.daily.dailyGoal, root.daily.reviewedToday, root.daily.dueCount,
                              tomorrow ? tomorrow.count : 0, AppTheme.colourPanel, AppTheme.colourMuted,
                              root.reviewColour, root.learnColour)
    }

    readonly property int doneToday: root.daily.learnedToday

    readonly property real goalRatio: root.daily.dailyGoal > 0
        ? Math.min(root.doneToday / root.daily.dailyGoal, 1.0)
        : 0.0

    readonly property bool goalReached: root.goalRatio >= 1
    readonly property bool reviewReady: root.hasReviewSession || root.daily.dueCount > 0

    readonly property color reviewColour: AppTheme.colourAccent
    readonly property color learnColour: AppTheme.colourCorrect

    readonly property var weekDays:
    {
        const locale = Qt.locale(AppLocalization.localeName)
        const dayName = function(offset)
        {
            const date = new Date(root.daily.learningDate)
            date.setDate(date.getDate() + offset)

            return locale.dayName(date.getDay(), Locale.ShortFormat)
        }

        const days = []

        for (const entry of root.daily.recentProgress)
        {
            if (entry.daysAgo === 0)
            {
                continue
            }

            days.push({ kind: "past", name: dayName(-entry.daysAgo), recorded: entry.recorded,
                        review: entry.review, learn: entry.learn })
        }

        days.push({ kind: "today", name: dayName(0) })

        for (const entry of root.daily.reviewTimeline)
        {
            if (entry.daysFromToday === 0)
            {
                continue
            }

            days.push({ kind: "coming", name: dayName(entry.daysFromToday), count: entry.count,
                        daysFromToday: entry.daysFromToday })
        }

        return days
    }

    readonly property int reviewTotal: root.daily.reviewedToday + root.daily.dueCount
    readonly property int dueCount: root.daily.dueCount
    readonly property int wordsLeftForGoal: root.daily.wordsLeftForGoal
    readonly property real reviewRatio: root.reviewTotal > 0 ? root.daily.reviewedToday / root.reviewTotal : 1.0

    property int shownDoneToday: root.doneToday
    property real shownGoalRatio: root.goalRatio

    Behavior on shownDoneToday
    {
        NumberAnimation { duration: AppMetrics.motionSlow; easing.type: AppMetrics.motionEasing }
    }

    Behavior on shownGoalRatio
    {
        NumberAnimation { duration: AppMetrics.motionSlow; easing.type: AppMetrics.motionEasing }
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

    readonly property bool compactPhone: root.height < 840

    readonly property var continueBooks: root.reader ? Shelf.continueBooks(root.reader.documents) : []
    readonly property var startBooks: root.reader && root.continueBooks.length === 0
        ? Shelf.startBooks(root.reader.freeBooks, root.vocabularyTotal) : []
    readonly property var shownBook: root.continueBooks.length > 0 ? root.continueBooks[0]
                                   : root.startBooks.length > 0 ? root.startBooks[0] : null
    readonly property bool shownBookFree: root.continueBooks.length === 0 && root.shownBook !== null

    readonly property int learnRemaining: root.hasLearnSession
        ? root.intensive.controller.wordCountFor(root.intensive.controller.vocabularyKey)
        : Math.max(root.daily.dailyGoal - root.shownDoneToday, 0)

    MobileShell
    {
        anchors.fill: parent

        activeTab: "today"
        contentMaximumWidth: AppMetrics.shellMaximumWidth
        headerTitle: root.store.targetLanguageName
        headerTitleTappable: true
        showSearch: true
        showSettings: true

        onHeaderTitleTapped: languageSheet.open()

        onTabSelected: function(key) { root.tabSelected(key) }
        onSearchRequested: root.dictionaryRequested()
        onSettingsRequested: root.settingsRequested()

        Flickable
        {
            id: scroll

            anchors.fill: parent
            contentHeight: content.implicitHeight + 16
            interactive: contentHeight > height
            boundsBehavior: Flickable.StopAtBounds
            clip: true

            ColumnLayout
            {
                id: content

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                spacing: 12

                MobileReconnectBanner
                {
                    objectName: "today_reconnect_banner"

                    Layout.leftMargin: 12
                    Layout.rightMargin: 12
                }

                MobileWeekStrip
                {
                    objectName: "today_week"

                    Layout.topMargin: AppMetrics.gapSmall

                    days: root.weekDays
                    todayReview: root.reviewRatio
                    todayLearn: root.shownGoalRatio
                    showTodayReview: root.reviewTotal > 0
                    showTodayLearn: root.daily.dailyGoal > 0
                    reviewColour: root.reviewColour
                    learnColour: root.learnColour

                    onDayRequested: function(daysFromToday) { root.wordListRequested("due:" + daysFromToday) }
                }

                MobileStudyBand
                {
                    id: reviewBand
                    objectName: "today_review"

                    Layout.leftMargin: 12
                    Layout.rightMargin: 12

                    label: root.hasReviewSession ? qsTr("Continue") : qsTr("Review")
                    count: root.daily.dueCount
                    unit: qsTr("due")
                    progress: root.reviewTotal > 0 ? root.reviewRatio : -1
                    active: root.reviewReady
                    compactPhone: root.compactPhone
                    tone: AppTheme.colourReviewBand
                    ink: AppTheme.colourReviewInk
                    deep: AppTheme.colourReviewDeep

                    onClicked: root.repetitionRequested()
                }

                MobileStudyBand
                {
                    id: learnBand
                    objectName: "today_learn"

                    Layout.leftMargin: 12
                    Layout.rightMargin: 12

                    label: root.hasLearnSession ? qsTr("Continue") : qsTr("Learn")
                    count: root.learnRemaining
                    counted: root.hasLearnSession || root.daily.dailyGoal > 0
                    unit: root.learnRemaining > 0 || root.hasLearnSession ? qsTr("new") : qsTr("Goal reached")
                    active: !root.reviewReady
                    compactPhone: root.compactPhone
                    tone: AppTheme.colourLearnBand
                    ink: AppTheme.colourLearnInk
                    deep: AppTheme.colourLearnDeep

                    onClicked: root.startFromPool()
                }

                RowLayout
                {
                    Layout.fillWidth: true
                    Layout.leftMargin: 12
                    Layout.rightMargin: 12
                    spacing: AppMetrics.gapSmall

                    MobileTodayTile
                    {
                        objectName: "today_discover"

                        label: qsTr("Discover")
                        iconSvg: Icons.Icons.nav_decks.path
                        primary: !!root.discovery && root.discovery.hasActiveSession
                        compactPhone: root.compactPhone

                        onClicked: root.discoverRequested()
                    }

                    MobileTodayTile
                    {
                        objectName: "today_practice"

                        label: qsTr("Practice")
                        iconSvg: Icons.Icons.exercise_flashcards.path
                        locked: root.vocabularyTotal === 0
                        compactPhone: root.compactPhone

                        onClicked: root.practiceRequested()
                        onBlockedClicked: root.messageRequested(qsTr("Learn some words to practise them"))
                    }

                    MobileTodayTile
                    {
                        objectName: "today_sentences"

                        label: qsTr("Sentences")
                        iconSvg: Icons.Icons.session_sentences.path
                        primary: !!root.sentences && root.sentences.savedExercises.length > 0
                        locked: !root.sentencesReady
                        compactPhone: root.compactPhone

                        onClicked: root.sentencesRequested()
                        onBlockedClicked: root.messageRequested(root.sentencesBlockedReason)
                    }
                }

                MobileTodayBook
                {
                    objectName: "today_book"

                    Layout.leftMargin: 12
                    Layout.rightMargin: 12

                    book: root.shownBook
                    level: root.shownBookFree ? Shelf.levelLabel(root.shownBook.level) || qsTr("Free book") : ""
                    needsDownload: root.shownBookFree && !root.shownBook.local
                    downloadProgress:
                    {
                        if (!root.shownBookFree || root.shownBook.local || !root.reader)
                        {
                            return -1
                        }

                        const value = root.reader.freeTransfers[root.shownBook.freeId]

                        return value === undefined ? -1 : value
                    }

                    onClicked:
                    {
                        if (root.shownBook === null)
                        {
                            root.tabSelected("reader")

                            return
                        }

                        if (root.shownBookFree)
                        {
                            root.freeBookRequested(root.shownBook)

                            return
                        }

                        root.bookRequested(root.shownBook)
                    }
                }
            }

            Item
            {
                id: cat

                readonly property Item band: root.reviewReady ? reviewBand : learnBand
                readonly property real headroom: band.heroHeight - band.catHeight + 32
                readonly property real sideRoom: 8
                readonly property real riseOffset: band.catRise * band.catHeight
                readonly property real pictureWidth: Math.round(band.catHeight * 535 / 675)
                readonly property real fallbackScale: pictureWidth / 508.6

                x: content.x + band.x + band.width - pictureWidth - 10 - sideRoom
                y: content.y + band.y + band.height - height
                width: pictureWidth + 2 * sideRoom
                height: band.catHeight + headroom
                visible: riseOffset < band.catHeight
                clip: true
                z: 1

                Image
                {
                    x: cat.sideRoom - 83.7 * cat.fallbackScale
                    y: cat.headroom + cat.riseOffset + 21.9 * cat.fallbackScale
                    width: 676 * cat.fallbackScale
                    height: 1214 * cat.fallbackScale
                    visible: !animation.ready
                    source: "qrc:/images/LufiExcited.svg"
                    sourceSize: Qt.size(width, height)
                }

                Lufi
                {
                    id: animation
                    objectName: "today_lufi"

                    anchors.fill: parent
                    todayActive: root.lufiActive && Qt.application.state === Qt.ApplicationActive
                        && cat.y + cat.height > scroll.contentY && cat.y < scroll.contentY + scroll.height
                    goalComplete: root.goalReached || (root.daily.dailyGoal <= 0 && root.reviewTotal > 0 && root.reviewRatio >= 1)
                    daily: root.daily
                    training: root.intensive ? root.intensive.controller : null
                    review: root.repetition ? root.repetition.controller : null
                    tapEnabled: false
                    crop: Qt.rect(-cat.sideRoom / cat.pictureWidth, -(cat.headroom + cat.riseOffset) / cat.band.catHeight,
                                  cat.width / cat.pictureWidth, cat.height / cat.band.catHeight)
                }

                MouseArea
                {
                    y: Math.max(0, cat.headroom + cat.riseOffset - cat.band.catHeight / 7)
                    width: parent.width
                    height: Math.max(0, parent.height - y)
                    onClicked: animation.reactToTap()
                }
            }
        }
    }

    MobileLanguageSheet
    {
        id: languageSheet

        anchors.fill: parent
        z: 60

        store: root.store
        installedOnly: true
    }
}
