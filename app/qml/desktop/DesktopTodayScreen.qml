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

    signal learnRequested()
    signal reviewRequested()
    signal formsTrainingRequested(string training)
    signal sentencesRequested()
    signal practiceRequested()
    signal wordListRequested(string source)
    signal messageRequested(string message)

    property bool hasLearnSession: false
    readonly property bool hasReviewSession: root.review ? root.review.hasActiveSession : false

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
                letter: locale.dayName((Qt.Monday + index) % 7, Locale.NarrowFormat),
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
            return root.sentences.available ? qsTr("Learn more words") : qsTr("No sentences yet")
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
    }

    Rectangle
    {
        anchors.fill: parent
        color: AppTheme.colourSunken
    }

    DesktopPage
    {
        id: page

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: notices.visible ? notices.top : parent.bottom
        topFill: AppMetrics.todayTopFill

        Item
        {
            id: board

            readonly property real sideInset: AppMetrics.gapSmall
            readonly property real restWidth: board.width - AppMetrics.todayRingColumnWidth - grid.columnSpacing * 2 - board.sideInset
            readonly property real sideWidth: Math.round(board.restWidth * AppMetrics.todaySideShare)
            readonly property real tileWidth: board.restWidth - board.sideWidth
            readonly property real leftShift: grammarColumn.visible ? 0
                : Math.round((grid.height - reviewTile.y - learnTile.y - learnTile.height) / 2)

            Layout.fillWidth: true
            implicitHeight: grid.implicitHeight

            DesktopTodayPath
            {
                anchors.fill: parent

                ringRight: ringCell.x + ring.x + ring.width
                ringY: ringCell.y + ring.y + ring.height / 2 + board.leftShift
                reviewY: reviewTile.y + reviewTile.height / 2 + board.leftShift
                learnY: learnTile.y + learnTile.height / 2 + board.leftShift
                grammarY: grammarColumn.y + grammarRow.y + grammarRow.height / 2
                tileLeft: reviewTile.x - AppMetrics.gapTight
                grammarLeft: grammarColumn.x
                practiceArrowX: (grammarColumn.x + grammarColumn.width + practiceColumn.x) / 2
                hasGrammar: grammarColumn.visible
                reviewDone: root.reviewDone
                learnDone: root.learnDone
                learnCurrent: root.reviewDone && !root.learnDone
            }

            GridLayout
            {
                id: grid

                width: board.width
                columns: 3
                columnSpacing: AppMetrics.gapWide
                rowSpacing: AppMetrics.gapCompact

                Item
                {
                    id: ringCell

                    transform: Translate { y: board.leftShift }

                    Layout.row: 0
                    Layout.column: 0
                    Layout.rowSpan: 2
                    Layout.preferredWidth: AppMetrics.todayRingColumnWidth
                    Layout.fillHeight: true

                    DesktopDailyRing
                    {
                        id: ring

                        x: -AppMetrics.todayRingOverhang
                        anchors.verticalCenter: parent.verticalCenter

                        outerProgress: root.reviewRatio
                        innerProgress: root.goalRatio
                        showOuter: root.hasReviewGoal
                        showInner: root.hasLearnGoal
                        doneToday: root.ringDone
                        dailyGoal: root.ringGoal
                    }
                }

                DesktopSessionTile
                {
                    id: reviewTile

                    objectName: "today_review"

                    transform: Translate { y: board.leftShift }

                    Layout.row: 0
                    Layout.column: 1
                    Layout.fillWidth: false
                    Layout.preferredWidth: board.tileWidth

                    primary: true
                    iconName: "session_review"
                    dotColour: AppTheme.colourAccent
                    text: root.hasReviewSession ? qsTr("Continue") : qsTr("Review")
                    count: daily.dueCount.toString()
                    completed: root.reviewDone
                    blocked: root.reviewDone

                    onActivated: root.reviewRequested()
                    onBlockedClicked: root.messageRequested(daily.reviewedToday > 0 ? qsTr("All reviewed for today")
                                                                                    : qsTr("Nothing is due for review yet"))
                }

                ColumnLayout
                {
                    Layout.row: 0
                    Layout.column: 2
                    Layout.preferredWidth: board.sideWidth
                    Layout.leftMargin: board.sideInset
                    Layout.alignment: Qt.AlignVCenter
                    spacing: AppMetrics.gapSmall

                    DesktopRuledLabel
                    {
                        text: qsTr("Coming up")
                    }

                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: AppMetrics.gapSmall

                        Repeater
                        {
                            model: daily.reviewTimeline

                            delegate: DesktopDayTile
                            {
                                required property var modelData

                                Layout.fillWidth: true
                                Layout.preferredWidth: 1

                                count: modelData.count
                                text: root.dayLabel(modelData.daysFromToday)
                                caption: root.shortDayLabel(modelData.daysFromToday)

                                onClicked: root.wordListRequested("due:" + modelData.daysFromToday)
                            }
                        }
                    }
                }

                DesktopSessionTile
                {
                    id: learnTile

                    objectName: "today_learn"

                    transform: Translate { y: board.leftShift }

                    Layout.row: 1
                    Layout.column: 1
                    Layout.fillWidth: false
                    Layout.preferredWidth: board.tileWidth

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

                ColumnLayout
                {
                    Layout.row: 1
                    Layout.column: 2
                    Layout.preferredWidth: board.sideWidth
                    Layout.leftMargin: board.sideInset
                    Layout.alignment: Qt.AlignVCenter
                    spacing: AppMetrics.gapSmall

                    DesktopRuledLabel
                    {
                        text: qsTr("This week")
                        trailing: root.learnedThisWeek.toString()
                    }

                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: 0

                        Repeater
                        {
                            model: root.weekDays

                            delegate: DesktopWeekDay
                            {
                                required property var modelData

                                Layout.fillWidth: true
                                Layout.preferredWidth: 1

                                letter: modelData.letter
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
                    id: grammarColumn

                    visible: root.formsTrainings.length > 0

                    Layout.row: 2
                    Layout.column: 1
                    Layout.preferredWidth: board.tileWidth
                    Layout.topMargin: AppMetrics.todayPracticeGap
                    spacing: AppMetrics.gapSmall

                    DesktopRuledLabel
                    {
                        text: qsTr("Grammar")
                    }

                    RowLayout
                    {
                        id: grammarRow

                        Layout.fillWidth: true
                        spacing: AppMetrics.gapSmall

                        Repeater
                        {
                            id: formsTiles

                            model: root.formsTrainings

                            delegate: DesktopSessionTile
                            {
                                required property var modelData

                                objectName: "today_" + modelData.key

                                Layout.preferredWidth: 1

                                compact: true
                                primary: modelData.resumable || root.nextPractice === modelData.key
                                iconName: modelData.icon
                                text: modelData.label
                                caption: root.formsCaption(modelData)
                                blocked: !modelData.ready

                                onActivated: root.formsTrainingRequested(modelData.key)
                                onBlockedClicked: root.messageRequested(modelData.blockedReason)
                            }
                        }
                    }
                }

                ColumnLayout
                {
                    id: practiceColumn

                    Layout.row: 2
                    Layout.column: 2
                    Layout.preferredWidth: board.sideWidth
                    Layout.leftMargin: board.sideInset
                    Layout.topMargin: AppMetrics.todayPracticeGap
                    spacing: AppMetrics.gapSmall

                    DesktopRuledLabel
                    {
                        text: qsTr("Practice")
                    }

                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: AppMetrics.gapSmall

                        DesktopSessionTile
                        {
                            id: sentencesTile

                            objectName: "today_sentences"

                            Layout.preferredWidth: 1

                            compact: true
                            primary: (root.sentences && root.sentences.savedExercises.length > 0) || root.nextPractice === "sentences"
                            iconName: "session_sentences"
                            text: qsTr("Sentences")
                            caption: root.sentencesCaption()
                            blocked: !root.sentencesReady

                            onActivated: root.sentencesRequested()
                            onBlockedClicked: root.messageRequested(root.sentences && !root.sentences.available
                                                                    ? qsTr("This dictionary has no sentences yet")
                                                                    : qsTr("Learn more words to unlock sentences"))
                        }

                        DesktopSessionTile
                        {
                            objectName: "today_practice"

                            Layout.preferredWidth: 1

                            compact: true
                            iconName: "exercise_flashcards"
                            text: qsTr("Practice")
                            caption: root.vocabularyTotal > 0 ? qsTr("%n words", "", root.vocabularyTotal)
                                                              : qsTr("Learn some words")
                            blocked: root.vocabularyTotal === 0

                            onActivated: root.practiceRequested()
                            onBlockedClicked: root.messageRequested(qsTr("Learn some words to practise them"))
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
            Layout.preferredWidth: page.columnWidth
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: AppMetrics.contentPadding - AppMetrics.gapSmall
        }
    }
}
