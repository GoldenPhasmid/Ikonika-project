import QtQuick
import QtQuick.Layouts
import "MobileIcons.js" as Icons

Item
{
    id: root

    required property var store
    property var repetition: null
    property var intensive: null
    property var sentences: null
    property var discovery: null

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
    signal formsTrainingRequested(string training)
    signal dictionaryRequested()
    signal settingsRequested()
    signal wordListRequested(string source)
    signal wordLookupRequested(string term)
    signal tabSelected(string key)

    function startFromPool()
    {
        root.intensiveRequested()
    }

    readonly property var formsTrainingKinds: [
        { key: "conjugation", iconSvg: Icons.Icons.session_conjugation.path, label: qsTr("Conjugations") },
        { key: "forms", iconSvg: Icons.Icons.session_forms.path, label: qsTr("Forms") }
    ]

    property var formsTrainings: []

    readonly property int vocabularyTotal: daily.ladder.reduce((Sum, Entry) => Sum + Entry.count, 0)

    readonly property var practiceItems:
    {
        const items = []

        for (const training of root.formsTrainings)
        {
            items.push({
                key: training.key,
                iconSvg: training.iconSvg,
                label: training.label,
                caption: training.resumable ? qsTr("Continue") : "",
                resumable: training.resumable,
                count: training.wordCount >= 0 ? training.wordCount.toString() : "",
                ready: training.ready,
                blockedReason: root.lockedHint
            })
        }

        items.push({
            key: "sentences",
            iconSvg: Icons.Icons.session_sentences.path,
            label: qsTr("Sentences"),
            caption: root.sentences && root.sentences.savedExercises.length > 0 ? qsTr("Continue") : "",
            resumable: root.sentences && root.sentences.savedExercises.length > 0,
            count: "",
            ready: root.sentencesReady,
            blockedReason: root.sentencesBlockedReason
        })

        items.push({
            key: "practice",
            iconSvg: Icons.Icons.exercise_flashcards.path,
            label: qsTr("Practice"),
            caption: "",
            resumable: false,
            count: root.vocabularyTotal.toString(),
            ready: root.vocabularyTotal > 0,
            blockedReason: qsTr("Learn some words to practise them")
        })

        return items
    }

    readonly property var readyPractice: root.practiceItems.filter(Item => Item.ready)
    readonly property var lockedPractice: root.practiceItems.filter(Item => !Item.ready)

    function startPractice(key)
    {
        if (key === "sentences")
        {
            root.sentencesRequested()

            return
        }

        if (key === "practice")
        {
            root.practiceRequested()

            return
        }

        root.formsTrainingRequested(key)
    }

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

            const resumable = root.intensive ? root.intensive.controller.hasSessionFor(trainingId) : false

            offered.push({
                key: kind.key,
                iconSvg: kind.iconSvg,
                label: kind.label,
                resumable: resumable,
                wordCount: resumable ? root.intensive.controller.wordCountFor(trainingId)
                                     : daily.formsWordCount(trainingId),
                ready: resumable || daily.canStartFormsTraining(trainingId)
            })
        }

        return offered
    }

    function refresh()
    {
        daily.refresh()
        root.hasLearnSession = root.findLearnSession()
        root.formsTrainings = root.listFormsTrainings()

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
        target: root.intensive ? root.intensive.controller : null
        enabled: root.visible

        function onStateChanged()
        {
            root.hasLearnSession = root.findLearnSession()
            root.formsTrainings = root.listFormsTrainings()
        }
    }

    DailySessionController
    {
        id: daily

        onCountsChanged: root.publishWidget()
        onFormsCountsChanged: root.formsTrainings = root.listFormsTrainings()
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
        const tomorrow = daily.reviewTimeline.find(Entry => Entry.daysFromToday === 1)

        AppHomeWidget.publish(root.doneToday, daily.dailyGoal, daily.reviewedToday, daily.dueCount,
                              tomorrow ? tomorrow.count : 0, AppTheme.colourPanel, AppTheme.colourMuted,
                              root.reviewColour, root.learnColour)
    }

    readonly property int doneToday: daily.learnedToday

    readonly property real goalRatio: daily.dailyGoal > 0
        ? Math.min(root.doneToday / daily.dailyGoal, 1.0)
        : 0.0

    readonly property bool goalReached: root.goalRatio >= 1
    readonly property bool reviewReady: root.hasReviewSession || daily.dueCount > 0

    readonly property color reviewColour: AppTheme.colourAccent
    readonly property color learnColour: AppTheme.colourCorrect

    readonly property var weekDays:
    {
        const locale = Qt.locale(AppLocalization.localeName)
        const todayIndex = daily.weekday - Qt.Monday
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
                learn: today ? root.shownGoalRatio : (recorded ? recorded.learn : 0)
            })
        }

        return days
    }

    readonly property int reviewTotal: daily.reviewedToday + daily.dueCount
    readonly property int dueCount: daily.dueCount
    readonly property int wordsLeftForGoal: daily.wordsLeftForGoal
    readonly property real reviewRatio: root.reviewTotal > 0 ? daily.reviewedToday / root.reviewTotal : 1.0

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

    MobileShell
    {
        anchors.fill: parent

        activeTab: "today"
        contentMaximumWidth: 632
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
            anchors.fill: parent
            contentHeight: content.implicitHeight + AppMetrics.contentPadding + AppMetrics.gapLarge
            clip: true

            ColumnLayout
            {
                id: content
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: AppMetrics.contentPadding
                anchors.topMargin: AppMetrics.contentPadding
                anchors.bottomMargin: AppMetrics.gapLarge
                spacing: AppMetrics.gapMedium

                MobileReconnectBanner
                {
                    objectName: "today_reconnect_banner"
                }

                RowLayout
                {
                    id: weekSection
                    Layout.fillWidth: true
                    spacing: 0

                    Repeater
                    {
                        model: root.weekDays

                        delegate: ColumnLayout
                        {
                            id: dayCell

                            required property var modelData

                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            spacing: AppMetrics.gapTight

                            Text
                            {
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignHCenter
                                text: dayCell.modelData.letter
                                font.pixelSize: AppMetrics.fontMeta
                                font.weight: dayCell.modelData.today ? Font.Bold : Font.Medium
                                font.family: AppMetrics.fontFamily
                                color: dayCell.modelData.today ? AppTheme.colourText : AppTheme.colourMuted
                            }

                            Rectangle
                            {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.preferredWidth: 40
                                Layout.preferredHeight: 40
                                radius: width / 2
                                color: dayCell.modelData.today ? AppTheme.colourFaint : "transparent"

                                MobileDoubleRing
                                {
                                    anchors.centerIn: parent
                                    width: 28
                                    height: 28
                                    visible: dayCell.modelData.today || dayCell.modelData.past

                                    stroke: 3
                                    outerProgress: dayCell.modelData.review
                                    innerProgress: dayCell.modelData.learn
                                    outerColour: root.reviewColour
                                    innerColour: root.learnColour
                                }
                            }
                        }
                    }
                }

                RowLayout
                {
                    id: doubleSection
                    Layout.fillWidth: true
                    spacing: AppMetrics.gapMedium

                    MobileDoubleRing
                    {
                        id: doubleRing
                        Layout.preferredWidth: 148
                        Layout.preferredHeight: 148

                        stroke: 14
                        outerProgress: root.reviewRatio
                        innerProgress: root.shownGoalRatio
                        showOuter: root.reviewTotal > 0
                        showInner: daily.dailyGoal > 0
                        outerColour: root.reviewColour
                        innerColour: root.learnColour

                        ColumnLayout
                        {
                            anchors.centerIn: parent
                            visible: daily.dailyGoal > 0
                            spacing: 0

                            Text
                            {
                                Layout.alignment: Qt.AlignHCenter
                                text: root.shownDoneToday
                                font.family: AppMetrics.fontFamily
                                font.pixelSize: AppMetrics.heroFigureSize
                                font.weight: Font.Bold
                                color: AppTheme.colourText
                            }

                            Text
                            {
                                Layout.alignment: Qt.AlignHCenter
                                text: qsTr("of %1").arg(daily.dailyGoal)
                                font.family: AppMetrics.fontFamily
                                font.pixelSize: AppMetrics.fontSmall
                                color: AppTheme.colourMuted
                            }
                        }
                    }

                    ColumnLayout
                    {
                        Layout.fillWidth: true
                        Layout.preferredHeight: doubleRing.height
                        spacing: AppMetrics.gapSmall

                        MobileRingTile
                        {
                            objectName: "today_review"

                            label: root.hasReviewSession ? qsTr("Continue") : qsTr("Review")
                            remaining: daily.dueCount
                            completed: !root.hasReviewSession && daily.dueCount === 0
                            accent: root.reviewColour
                            interactive: root.reviewReady

                            onClicked: root.repetitionRequested()
                        }

                        MobileRingTile
                        {
                            objectName: "today_learn"

                            label: root.hasLearnSession ? qsTr("Continue") : qsTr("Learn")
                            remaining: root.hasLearnSession ? root.intensive.controller.wordCountFor(root.intensive.controller.vocabularyKey)
                                                           : Math.max(daily.dailyGoal - root.shownDoneToday, 0)
                            showRemaining: root.hasLearnSession || daily.dailyGoal > 0
                            completed: !root.hasLearnSession && root.goalReached
                            accent: root.learnColour

                            onClicked: root.startFromPool()
                        }
                    }
                }

                MobileActionButton
                {
                    objectName: "today_discover"

                    readonly property bool resumable: !!root.discovery && root.discovery.hasActiveSession

                    filled: resumable
                    iconSvg: Icons.Icons.nav_decks.path
                    label: qsTr("Discover")
                    caption: resumable ? qsTr("Continue") : qsTr("Find new words")

                    onClicked: root.discoverRequested()
                }

                ColumnLayout
                {
                    id: dueSection
                    Layout.fillWidth: true
                    spacing: AppMetrics.gapCompact

                    MobileSectionLabel { text: qsTr("COMING UP") }

                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: AppMetrics.gapTight

                        Repeater
                        {
                            model: daily.reviewTimeline.slice(0, 5)

                            delegate: Rectangle
                            {
                                required property var modelData

                                Layout.fillWidth: true
                                Layout.preferredHeight: 64
                                readonly property bool tappable: modelData.count > 0

                                radius: AppMetrics.radiusMedium
                                color: Qt.tint(AppTheme.colourPanel,
                                    Qt.rgba(AppTheme.colourAccent.r, AppTheme.colourAccent.g,
                                            AppTheme.colourAccent.b,
                                            !tappable ? 0 : dueTap.pressed ? 0.24 : 0.10))
                                border.color: tappable
                                    ? Qt.rgba(AppTheme.colourAccent.r, AppTheme.colourAccent.g,
                                              AppTheme.colourAccent.b, 0.6)
                                    : AppTheme.colourBorder
                                border.width: AppMetrics.borderWidth
                                scale: dueTap.pressed ? 0.95 : 1

                                Behavior on scale
                                {
                                    NumberAnimation { duration: AppMetrics.motionFast; easing.type: AppMetrics.motionEasing }
                                }

                                TapHandler
                                {
                                    id: dueTap

                                    enabled: parent.tappable

                                    onTapped: root.wordListRequested("due:" + modelData.daysFromToday)
                                }

                                ColumnLayout
                                {
                                    anchors.centerIn: parent
                                    spacing: 2

                                    Text
                                    {
                                        text: modelData.count
                                        font.pixelSize: AppMetrics.fontSubtitle
                                        font.weight: Font.Bold
                                        font.family: AppMetrics.fontFamily
                                        color: modelData.count > 0 ? AppTheme.colourText : AppTheme.colourMuted
                                        Layout.alignment: Qt.AlignHCenter
                                    }

                                    Text
                                    {
                                        text:
                                        {
                                            if (modelData.daysFromToday === 0) return qsTr("Today")

                                            const date = new Date(daily.learningDate)
                                            date.setDate(date.getDate() + modelData.daysFromToday)

                                            return Qt.locale(AppLocalization.localeName).toString(date, "ddd")
                                        }
                                        font.pixelSize: AppMetrics.fontMeta
                                        font.family: AppMetrics.fontFamily
                                        color: AppTheme.colourMuted
                                        Layout.alignment: Qt.AlignHCenter
                                    }
                                }
                            }
                        }
                    }
                }

                ColumnLayout
                {
                    id: practiceSection
                    Layout.fillWidth: true
                    spacing: AppMetrics.gapCompact

                    MobileSectionLabel { text: qsTr("PRACTICE") }

                    Repeater
                    {
                        model: root.readyPractice

                        delegate: MobileActionButton
                        {
                            required property var modelData

                            objectName: "today_" + modelData.key

                            filled: modelData.resumable
                            iconSvg: modelData.iconSvg
                            label: modelData.label
                            caption: modelData.caption
                            trailing: modelData.count

                            onClicked: root.startPractice(modelData.key)
                        }
                    }

                    Repeater
                    {
                        model: root.lockedPractice

                        delegate: MobileLockedRow
                        {
                            required property var modelData

                            Layout.topMargin: AppMetrics.gapTight

                            label: modelData.label
                            reason: modelData.blockedReason
                        }
                    }
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
