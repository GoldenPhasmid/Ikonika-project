import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item
{
    id: root

    readonly property bool hasUpdates: (AppCatalog.updateAvailable && !AppUpdater.storePackage && !AppCatalog.appUpdateDismissed)
        || AppCatalog.packageUpdates.some(Update => !Update.dismissed)

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

    property bool hasLearnSession: false
    readonly property bool hasReviewSession: root.review ? root.review.hasActiveSession : false
    readonly property bool hasDiscoverSession: root.discovery ? root.discovery.hasActiveSession : false

    readonly property int wordsLeftForGoal: daily.wordsLeftForGoal

    readonly property int reviewCount: root.hasReviewSession ? root.review.cardCount : daily.dueCount

    readonly property bool sentencesReady: !!root.sentences
        && (root.sentences.savedExercises.length > 0 || root.sentences.unlocked)

    readonly property Item goalRing: ring
    readonly property int dailyGoal: daily.dailyGoal
    readonly property int doneToday: daily.learnedToday
    readonly property real goalRatio: daily.dailyGoal > 0 ? Math.min(root.doneToday / daily.dailyGoal, 1.0) : 0.0

    readonly property bool reviewDone: !root.hasReviewSession && daily.dueCount === 0
    readonly property bool hasLearnGoal: root.dailyGoal > 0
    readonly property bool learnDone: !root.hasLearnGoal || root.goalRatio >= 1
    readonly property int reviewTotal: daily.reviewedToday + daily.dueCount
    readonly property real reviewRatio: root.reviewTotal > 0 ? daily.reviewedToday / root.reviewTotal : 1.0
    readonly property bool hasReviewGoal: root.reviewTotal > 0
    readonly property int ringDone: root.hasLearnGoal ? root.doneToday : daily.reviewedToday
    readonly property int ringGoal: root.hasLearnGoal ? root.dailyGoal : root.reviewTotal

    readonly property var comingDays: daily.reviewTimeline.filter(Day => Day.daysFromToday > 0)

    readonly property int vocabularyTotal: daily.ladder.reduce((Sum, Entry) => Sum + Entry.count, 0)

    readonly property bool conjugationReady: root.formsTrainings.some(Training => Training.key === "conjugation" && Training.ready)

    readonly property string nextPractice:
    {
        if (!root.reviewDone || !root.learnDone)
        {
            return ""
        }

        if (root.conjugationReady)
        {
            return "conjugation"
        }

        return root.sentencesReady ? "sentences" : ""
    }

    function weekdayIndex()
    {
        return daily.weekday - Qt.Monday
    }

    readonly property int learnedThisWeek:
    {
        const todayIndex = root.weekdayIndex()

        return daily.recentProgress.filter(Entry => Entry.daysAgo <= todayIndex)
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
            const recorded = daily.recentProgress.find(Entry => Entry.daysAgo === todayIndex - index)

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
            const trainingId = daily.formsTrainingId(kind.key)

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
                wordCount: daily.formsWordCount(trainingId),
                ready: resumable || daily.canStartFormsTraining(trainingId)
            })
        }

        return offered
    }

    function refresh()
    {
        daily.refresh()
        root.hasLearnSession = root.training ? root.training.hasSessionFor(root.training.vocabularyKey) : false
        root.formsTrainings = root.listFormsTrainings()

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

            if (tile && tile.modelData.key === key)
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

        const date = new Date(daily.learningDate)
        date.setDate(date.getDate() + daysFromToday)

        return Qt.locale(AppLocalization.localeName).toString(date, "dddd")
    }

    function shortDayLabel(daysFromToday)
    {
        const date = new Date(daily.learningDate)
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
            root.formsTrainings = root.listFormsTrainings()
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

    DailySessionController
    {
        id: daily

        onFormsCountsChanged: root.formsTrainings = root.listFormsTrainings()
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

                DesktopDashedBox
                {
                    objectName: "today_cat"

                    width: Math.min(board.sideWidth, board.catRoom * AppMetrics.todayCatAspect)
                    height: width / AppMetrics.todayCatAspect
                    y: board.height - height
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
                        count: daily.dueCount.toString()
                        completed: root.reviewDone
                        blocked: root.reviewDone

                        onActivated: root.reviewRequested()
                        onBlockedClicked: root.messageRequested(daily.reviewedToday > 0 ? qsTr("All reviewed for today")
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
                                                   : (root.hasLearnGoal ? daily.wordsLeftForGoal.toString() : "")
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
                        caption: root.hasDiscoverSession ? qsTr("Continue") : qsTr("Find new words")

                        onActivated: root.discoverRequested()
                    }

                    DesktopSessionTile
                    {
                        id: sentencesTile

                        objectName: "today_sentences"

                        Layout.preferredHeight: sessions.unit
                        Layout.bottomMargin: root.formsTrainings.length > 0 ? 0 : AppMetrics.gapSmall

                        compact: true
                        primary: (root.sentences && root.sentences.savedExercises.length > 0) || root.nextPractice === "sentences"
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

                        model: root.formsTrainings

                        delegate: DesktopSessionTile
                        {
                            required property var modelData
                            required property int index

                            objectName: "today_" + modelData.key

                            Layout.preferredHeight: sessions.unit
                            Layout.bottomMargin: index === root.formsTrainings.length - 1 ? AppMetrics.gapSmall : 0

                            compact: true
                            primary: modelData.resumable || root.nextPractice === modelData.key
                            iconName: modelData.icon
                            text: modelData.label
                            caption: root.formsCaption(modelData)
                            count: root.formsCount(modelData)
                            blocked: !modelData.ready

                            onActivated: root.formsTrainingRequested(modelData.key)
                            onBlockedClicked: root.messageRequested(modelData.blockedReason)
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
                            text: qsTr("Continue reading")
                        }
                    }

                    RowLayout
                    {
                        spacing: AppMetrics.todayTileGap

                        Repeater
                        {
                            model: 2

                            delegate: DesktopDashedBox
                            {
                                Layout.preferredWidth: board.bookWidth
                                Layout.preferredHeight: board.bookWidth / AppMetrics.todayBookAspect
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

        visible: root.hasUpdates || AppSyncController.needsReconnect

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: AppMetrics.gapSmall
        spacing: AppMetrics.gapSmall

        Flickable
        {
            id: updates

            visible: root.hasUpdates
            Layout.fillWidth: true
            Layout.preferredHeight: updateRow.implicitHeight
            Layout.leftMargin: AppMetrics.gapSmall
            contentWidth: updateRow.implicitWidth
            contentHeight: height
            clip: true
            flickableDirection: Flickable.HorizontalFlick
            ScrollBar.horizontal: ScrollBar {}

            RowLayout
            {
                id: updateRow

                spacing: AppMetrics.gapSmall

                DesktopAppUpdateCard
                {
                    id: appUpdate

                    objectName: "today_app_update"
                    dismissible: true

                    Layout.fillWidth: false
                    Layout.preferredWidth: AppMetrics.categoryPaneWidth
                    Layout.alignment: Qt.AlignBottom
                }

                Repeater
                {
                    model: AppCatalog.packageUpdates

                    delegate: DesktopPackageUpdateCard
                    {
                        required property var modelData

                        objectName: "today_" + modelData.id + "_update"
                        update: modelData
                        dismissible: true

                        Layout.fillWidth: false
                        Layout.preferredWidth: AppMetrics.categoryPaneWidth
                        Layout.alignment: Qt.AlignBottom
                    }
                }
            }
        }

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
