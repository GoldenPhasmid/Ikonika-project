import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Window

ApplicationWindow
{
    id: window

    width: AppMetrics.windowWidth
    height: AppMetrics.windowHeight
    minimumWidth: AppMetrics.windowMinimumWidth
    minimumHeight: AppMetrics.windowMinimumHeight
    visible: true
    title: qsTr("Ikonika")

    color: AppTheme.colourPanel

    font.family: AppMetrics.fontFamily
    font.pixelSize: AppMetrics.fontBody

    palette.window: AppTheme.colourPanel
    palette.windowText: AppTheme.colourText
    palette.base: AppTheme.colourPanel
    palette.text: AppTheme.colourText
    palette.button: AppTheme.colourPanel
    palette.buttonText: AppTheme.colourText
    palette.highlight: AppTheme.colourAccent
    palette.highlightedText: AppTheme.colourAccentText
    palette.mid: AppTheme.colourBorder
    palette.dark: AppTheme.colourBorder
    palette.light: AppTheme.colourMuted
    palette.toolTipBase: AppTheme.colourText
    palette.toolTipText: AppTheme.colourPanel
    palette.placeholderText: AppTheme.colourMuted

    onClosing: function(close)
    {
        if (!AppReminder.trayAvailable)
        {
            return
        }

        close.accepted = false
        window.hide()
    }

    readonly property SettingsStore store: SettingsStore {}

    property string destination: "today"
    property string session: ""
    property string suspendedSession: ""
    property bool initialized: false
    property bool opened: false

    function openWelcome()
    {
        if (!window.initialized || !window.visible || !window.active || window.opened)
        {
            return
        }

        window.opened = true

        if (window.onboarding || window.walksRequested !== 0 || dictionaryBenchmark.requested || window.screen !== "today")
        {
            return
        }

        todayScreen.refresh()
        welcome.shown = true
        welcome.forceActiveFocus()
    }

    onActiveChanged: Qt.callLater(window.openWelcome)

    property string sessionOrigin: ""
    property Item morphSource: null
    property Item morphSurface: null
    property string returnOrigin: ""
    readonly property string deckOrigin: "deck:"
    readonly property string deckLearnOrigin: "deckLearn"

    readonly property bool onboarding: AppOnboarding.required
    readonly property bool focusMode: window.session.length > 0 || window.onboarding
    readonly property bool inSessionScreen: !window.onboarding && window.opensSessionScreen(window.session)
    readonly property bool immersive: window.screen === "reader" && readerScreen.immersive
    readonly property bool readingBook: window.screen === "reader" && readerScreen.readable
    property int visibilityBeforeFullScreen: Window.Windowed

    readonly property var sessions: ({
        "training": training,
        "review": review,
        "sentences": sentenceTraining,
        "practice": practice
    })

    readonly property var activeSession: window.sessions[window.session] || null

    readonly property DesktopScreenMorph activeMorph: triageScreen.morph.morphing ? triageScreen.morph : sessionScreen.morph

    readonly property string screen: window.focusMode && !window.activeMorph.morphing ? window.session : window.destination

    readonly property var pageOrder: topBar.destinations.map(Item => Item.key).concat(["settings"])
    readonly property bool sliding: slideAnimation.running
    property string shownPage: ""
    property string slideFrom: ""
    property string slideTo: ""
    property real slideFromStart: 0
    property real slideFromEnd: 0
    property real slideToStart: 0
    property real slideProgress: 1

    readonly property var crumbs:
    {
        if (window.destination === "vocabulary")
        {
            return [qsTr("My Vocabulary")]
        }

        if (window.destination === "decks")
        {
            return decksScreen.deckOpen ? [qsTr("Decks"), decksScreen.deckName] : [qsTr("Decks")]
        }

        if (window.destination === "words")
        {
            return window.suspendedSession.length > 0 ? [window.suspendedTitle(), qsTr("Words")] : [qsTr("Words")]
        }

        if (window.destination === "reader")
        {
            return readerScreen.reading ? [qsTr("Reader"), readerScreen.currentTitle] : [qsTr("Reader")]
        }

        if (window.destination === "settings")
        {
            return [qsTr("Settings")]
        }

        return [qsTr("Today")]
    }

    readonly property var testHooks: ({
        today: todayScreen,
        vocabulary: vocabularyScreen,
        decks: decksScreen,
        words: wordsScreen,
        reader: readerScreen,
        settings: settingsScreen,
        session: sessionScreen,
        training: training,
        review: review,
        triage: triageScreen,
        sentences: sentenceTraining,
        practice: practice,
        wordDialog: wordDialog
    })

    function navigate(key)
    {
        if (key === "account")
        {
            window.navigate("settings")
            settingsScreen.open("account")

            return
        }

        window.suspendedSession = ""
        window.destination = key

        if (key === "vocabulary")
        {
            vocabularyScreen.resetView()
        }

        if (key === "decks")
        {
            decksScreen.closeDeck()
        }

        window.refreshCurrentScreen()
    }

    function openPendingBook()
    {
        if (window.onboarding)
        {
            return
        }

        const book = AppReaderIntake.takePending()
        if (!book.path)
        {
            return
        }

        window.session = ""
        window.navigate("reader")
        readerScreen.openPath(book.path)
    }

    onOnboardingChanged: Qt.callLater(window.openPendingBook)

    Connections
    {
        target: AppReaderIntake
        function onBookArrived() { window.openPendingBook() }
    }

    function pageShown(key)
    {
        return window.screen === key || (window.sliding && key === window.slideFrom)
    }

    function pageOffset(key)
    {
        if (!window.sliding)
        {
            return 0
        }

        if (key === window.slideTo)
        {
            return window.slideToStart * (1 - window.slideProgress)
        }

        if (key === window.slideFrom)
        {
            return window.slideFromStart + (window.slideFromEnd - window.slideFromStart) * window.slideProgress
        }

        return 0
    }

    function slidePages()
    {
        const previous = window.shownPage
        const from = window.pageOrder.indexOf(previous)
        const to = window.pageOrder.indexOf(window.screen)

        window.shownPage = window.screen

        if (from < 0 || to < 0 || from === to || AppMetrics.motionPage <= 0)
        {
            slideAnimation.stop()

            return
        }

        const direction = to > from ? 1 : -1
        const fromStart = window.pageOffset(previous)
        const toStart = window.sliding && window.screen === window.slideFrom ? window.pageOffset(window.screen)
                                                                             : direction * pages.width

        slideAnimation.stop()
        window.slideFrom = previous
        window.slideTo = window.screen
        window.slideFromStart = fromStart
        window.slideFromEnd = -direction * pages.width
        window.slideToStart = toStart
        window.slideProgress = 0
        slideAnimation.start()
    }

    onScreenChanged: window.slidePages()

    function resetTo(name)
    {
        window.session = ""
        window.navigate(name === "dictionary" ? "words" : name)
    }

    function openVocabulary(filterKey)
    {
        window.navigate("vocabulary")
        vocabularyScreen.vocabulary.filterKeys = [filterKey]
    }

    function openWordList(source)
    {
        window.navigate("words")
        wordsScreen.openList(source)
    }

    function openDictionaryFromSession(word, partOfSpeech)
    {
        const suspended = window.session

        window.session = ""
        window.navigate("words")
        window.suspendedSession = suspended
        wordsScreen.showWord(word, partOfSpeech)
    }

    function openDictionary(word, partOfSpeech)
    {
        if (window.session.length > 0)
        {
            window.openDictionaryFromSession(word, partOfSpeech)

            return
        }

        if (window.screen !== "words")
        {
            window.navigate("words")
        }

        wordsScreen.showWord(word, partOfSpeech)
    }

    function suspendedTitle()
    {
        switch (window.suspendedSession)
        {
        case "triage": return qsTr("New words")
        case "review": return qsTr("Review")
        case "sentences": return qsTr("Sentences")
        case "practice": return qsTr("Practice")
        }

        return qsTr("Learning")
    }

    function resumeSuspendedSession()
    {
        const suspended = window.suspendedSession

        window.suspendedSession = ""
        window.enterSession(suspended)
    }

    function lookUpWord(term)
    {
        window.navigate("words")
        wordsScreen.searchTerm(term)
        wordsScreen.focusSearch()
    }

    function focusSearch()
    {
        if (window.focusMode)
        {
            return
        }

        if (window.screen === "vocabulary")
        {
            vocabularyScreen.focusSearch()

            return
        }

        if (window.readingBook)
        {
            return
        }

        if (window.screen === "reader" && !readerScreen.reading)
        {
            readerScreen.focusSearch()

            return
        }

        window.navigate("words")
        wordsScreen.focusSearch()
    }

    function createItem()
    {
        if (window.focusMode)
        {
            return
        }

        if (window.screen === "vocabulary")
        {
            window.addWordToVocabulary()

            return
        }

        if (window.screen === "words")
        {
            window.addWord(wordsScreen.term, "", "")

            return
        }

        if (window.screen === "reader" && !readerScreen.reading)
        {
            readerScreen.openBook()
        }
    }

    function addWord(word, translation, partOfSpeech, meaningGist, hint)
    {
        wordDialog.controller = vocabularyScreen.vocabulary
        wordDialog.openWith(word, translation, partOfSpeech, meaningGist || "", hint || "")
    }

    function addWordToVocabulary()
    {
        wordDialog.controller = vocabularyScreen.vocabulary
        wordDialog.openWith("", "", "")
    }

    function editWord(controller, cardId, word, translation, gender, partOfSpeech, box)
    {
        wordDialog.controller = controller
        wordDialog.openForEdit(cardId, word, translation, gender, partOfSpeech, box)
    }

    function refreshCurrentScreen()
    {
        window.store.clearLastError()
        daily.refresh()

        if (window.screen === "today")
        {
            todayScreen.refresh()
        }
        else if (window.screen === "vocabulary")
        {
            vocabularyScreen.refresh()
        }
        else if (window.screen === "decks")
        {
            decksScreen.refresh()
        }
        else if (window.screen === "words")
        {
            wordsScreen.refresh()
        }
        else if (window.screen === "reader")
        {
            readerScreen.refresh()
        }
    }

    function opensSessionScreen(kind)
    {
        return kind.length > 0 && kind !== "triage" && kind !== "placement"
    }

    function morphFor(kind)
    {
        if (window.opensSessionScreen(kind))
        {
            return sessionScreen.morph
        }

        return kind === "triage" ? triageScreen.morph : null
    }

    function startFrom(origin, source, surface, start)
    {
        window.sessionOrigin = origin
        window.morphSource = source
        window.morphSurface = surface

        start()

        window.sessionOrigin = ""
        window.morphSource = null
        window.morphSurface = null
    }

    function startFromTile(origin, start)
    {
        const tile = todayScreen.sessionTile(origin)

        window.startFrom(origin, tile, tile ? tile.background : null, start)
    }

    function startFromDeck(deckId, face)
    {
        window.startFrom(window.deckOrigin + deckId, face, face, function() { window.startDeckTriage(deckId) })
    }

    function returnTarget()
    {
        if (window.destination === "today")
        {
            const tile = todayScreen.sessionTile(window.returnOrigin)

            return tile ? { item: tile, surface: tile.background } : null
        }

        if (window.destination === "decks" && window.returnOrigin.startsWith(window.deckOrigin))
        {
            const face = decksScreen.deckFace(Number(window.returnOrigin.slice(window.deckOrigin.length)))

            return face ? { item: face, surface: face } : null
        }

        if (window.destination === "decks" && window.returnOrigin === window.deckLearnOrigin && decksScreen.deckOpen)
        {
            return { item: deckLearnButton, surface: deckLearnButton.background, rect: window.barRect(deckLearnButton) }
        }

        return null
    }

    function barRect(button)
    {
        return function(host)
        {
            const inBar = button.mapToItem(topBar, 0, 0, button.width, button.height)
            const bar = topBar.parent.mapToItem(host, topBar.x, topBar.y)

            return Qt.rect(bar.x + inBar.x, bar.y + inBar.y, inBar.width, inBar.height)
        }
    }

    function enterSession(kind)
    {
        window.returnOrigin = window.session === "triage" ? window.returnOrigin : window.sessionOrigin

        const morph = window.morphFor(kind)

        if (morph)
        {
            morph.prepareIn(window.morphSource, window.morphSurface)
        }

        window.session = kind

        if (morph && morph.morphing)
        {
            console.info("Session", kind, "opens from", window.sessionOrigin)
            Qt.callLater(morph.startIn)
        }
    }

    function leaveSession()
    {
        if (window.activeMorph.morphing)
        {
            return
        }

        AppReminder.syncDailyProgress()

        const morph = window.onboarding ? null : window.morphFor(window.session)
        const target = morph ? window.returnTarget() : null

        if (target && morph.ready)
        {
            const refreshOnLanding = window.destination === "decks"

            console.info("Session", window.session, "closes into", window.returnOrigin)
            morph.startOut(target.item, target.surface, function()
            {
                window.session = ""

                if (refreshOnLanding)
                {
                    window.refreshCurrentScreen()
                }
            }, target.rect)

            if (!refreshOnLanding)
            {
                window.refreshCurrentScreen()
            }

            return
        }

        window.session = ""
        window.refreshCurrentScreen()
    }

    function endSession()
    {
        window.activeSession.endSession()
        window.leaveSession()
    }

    function startTraining()
    {
        if (training.resume(training.vocabularyKey))
        {
            window.enterSession("training")

            return
        }

        if (window.startTriage())
        {
            return
        }

        window.startCollectedTraining()
    }

    function startTriage()
    {
        const missing = daily.shortfall()

        if (missing === 0 || !triage.start(missing))
        {
            return false
        }

        window.enterSession("triage")

        return true
    }

    function startDeckTriage(deckId)
    {
        if (!triage.startDeck(deckId))
        {
            toast.show(qsTr("Every word of this deck is already in My Vocabulary or known."))

            return
        }

        window.enterSession("triage")
    }

    function startCollectedTraining()
    {
        if (training.startCards(training.vocabularyKey, daily.collectSessionCards()))
        {
            window.enterSession("training")

            return
        }

        window.session = ""
        toast.show(qsTr("There are not enough words to start a session. Add more words first."))
    }

    function startSentences(exercise)
    {
        if (sentenceTraining.start(exercise))
        {
            window.enterSession("sentences")

            return
        }

        toast.show(qsTr("There are not enough sentences for these settings yet."))
    }

    function startPractice(exercise, cardCount, repetitionCount, filterKeys, reverse)
    {
        if (launcher.startExercise(exercise, reverse, cardCount, repetitionCount, filterKeys))
        {
            window.enterSession("practice")

            return
        }

        toast.show(qsTr("My Vocabulary has no words to practise yet."))
    }

    function startReview()
    {
        if (review.hasActiveSession || review.startDueToday())
        {
            window.enterSession("review")

            return
        }

        toast.show(qsTr("Nothing is due for review yet"))
    }

    function startFormsTraining(kind)
    {
        const trainingId = daily.formsTrainingId(kind)

        if (trainingId.length === 0)
        {
            return
        }

        if (training.resume(trainingId))
        {
            window.enterSession("training")

            return
        }

        const tenses = daily.formsTenses(trainingId)

        if (tenses.length > 1)
        {
            tenseDialog.origin = window.sessionOrigin
            tenseDialog.trainingId = trainingId
            tenseDialog.tenses = tenses
            tenseDialog.open()

            return
        }

        window.startTenseTraining(trainingId, "present")
    }

    function startTenseTraining(trainingId, tense)
    {
        const cards = tense === "present" ? daily.collectFormsCards(trainingId)
                                          : daily.collectTenseCards(trainingId, tense)

        if (training.resume(trainingId) || training.startCards(trainingId, cards))
        {
            window.enterSession("training")

            return
        }

        toast.show(qsTr("There are not enough words to start this training yet."))
    }

    function askToRemoveCard(cardId, word)
    {
        confirmDialog.ask(qsTr("Remove \"%1\"?").arg(word),
                          qsTr("The word leaves My Vocabulary and its progress is lost."),
                          qsTr("Remove"),
                          function()
                          {
                              vocabularyScreen.removeCard(cardId)

                              if (window.screen === "decks")
                              {
                                  decksScreen.refresh()
                              }
                          })
    }

    NumberAnimation
    {
        id: slideAnimation

        target: window
        property: "slideProgress"
        from: 0
        to: 1
        duration: AppMetrics.motionPage
        easing.type: AppMetrics.motionEasing
    }

    Component.onCompleted:
    {
        window.shownPage = window.screen

        const lastSession = training.lastActiveKey()

        if (lastSession.length > 0)
        {
            training.resume(lastSession)
        }

        if (dictionaryBenchmark.requested)
        {
            window.navigate("words")
            dictionaryBenchmark.start()
        }

        window.refreshCurrentScreen()
        screenWalk.start()
        navigationWalk.start()

        window.initialized = true
        Qt.callLater(window.openWelcome)

        const snapshotDirectory = snapshotProbe.environmentVariable("IKONIKA_SNAPSHOT_DIR")

        if (snapshotDirectory.length > 0)
        {
            snapshotProbe.serveSnapshots(window.contentItem, snapshotDirectory)
        }
    }

    UiProbe
    {
        id: snapshotProbe
    }

    DailySessionController
    {
        id: daily
    }

    IntensiveLearningController
    {
        id: training
    }

    RepetitionController
    {
        id: review
    }

    TriageController
    {
        id: triage

        onProgressChanged:
        {
            if (triage.finished && window.session === "triage")
            {
                Qt.callLater(triage.deckMode ? window.leaveSession : window.startCollectedTraining)
            }
        }
    }

    SentenceTrainingController
    {
        id: sentenceTraining
    }

    PlacementTestController
    {
        id: placementTest
    }

    ExerciseLauncherController
    {
        id: launcher
    }

    QtObject
    {
        id: practice

        readonly property var activeController: launcher.activeController
        readonly property bool finished: !!practice.activeController && practice.activeController.finished
        readonly property int stage: 1
        readonly property int stageCount: 1

        function restartStage()
        {
            practice.activeController.restart()
        }

        function completeStage()
        {
        }

        function endSession()
        {
        }
    }

    RowLayout
    {
        anchors.fill: parent
        spacing: 0
        visible: !window.focusMode || window.activeMorph.morphing

        ColumnLayout
        {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            DesktopTopBar
            {
                id: topBar

                Layout.fillWidth: true
                visible: !window.immersive
                opacity: window.activeMorph.morphing ? 1 - window.activeMorph.shellBarHidden : 1
                transform: Translate { y: window.activeMorph.morphing ? -topBar.height * window.activeMorph.shellBarHidden : 0 }

                store: window.store
                destination: window.destination
                dueCount: daily.dueCount
                crumbs: window.crumbs
                languageShortcut: languageSwitchShortcut.enabled

                onDestinationSelected: function(key) { window.navigate(key) }

                onCrumbActivated: function(index)
                {
                    if (index !== 0)
                    {
                        return
                    }

                    if (window.suspendedSession.length > 0)
                    {
                        window.resumeSuspendedSession()

                        return
                    }

                    if (window.screen === "reader" && readerScreen.reading)
                    {
                        readerScreen.closeBook()

                        return
                    }

                    if (window.screen === "decks" && decksScreen.deckOpen)
                    {
                        decksScreen.closeDeck()
                        decksScreen.refresh()

                        return
                    }

                    window.navigate(window.destination)
                }

                DesktopButton
                {
                    visible: window.pageOrder.indexOf(window.screen) >= 0 && !window.readingBook
                    tone: "ghost"
                    iconName: "search"
                    text: qsTr("Search")
                    keyHint: qsTr("Ctrl+F")
                    hintOnControl: true

                    onClicked: window.focusSearch()
                }

                DesktopButton
                {
                    id: deckLearnButton

                    objectName: "topbar_deck_learn"

                    visible: window.screen === "decks" && decksScreen.deckOpen
                    tone: "primary"
                    iconName: "today_learn"
                    text: qsTr("Learn · %1").arg(decksScreen.deckNewCount)
                    enabled: decksScreen.deckNewCount > 0

                    onClicked: window.startFrom(window.deckLearnOrigin, deckLearnButton, deckLearnButton.background,
                                                function() { window.startDeckTriage(decksScreen.deckId) })
                }

                DesktopIconButton
                {
                    objectName: "topbar_reader_contents"

                    visible: window.readingBook
                    opacity: readerScreen.chromeShown
                    iconName: "contents"
                    shortcutKey: "T"
                    active: readerScreen.contentsOpen
                    text: qsTr("Contents (Ctrl+T)")

                    onClicked: readerScreen.toggleContents()
                }

                DesktopIconButton
                {
                    objectName: "topbar_reader_read_aloud"

                    visible: window.readingBook && readerScreen.readAloudOffered
                    opacity: readerScreen.chromeShown
                    iconName: "headphones"
                    active: readerScreen.readAloud.enabled
                    text: qsTr("Read aloud")

                    onClicked: readerScreen.readAloud.enabled = !readerScreen.readAloud.enabled
                }

                DesktopReaderAppearanceButton
                {
                    objectName: "topbar_reader_appearance"

                    visible: window.readingBook
                    opacity: readerScreen.chromeShown
                    pdf: readerScreen.isPdf
                }

                DesktopIconButton
                {
                    objectName: "topbar_reader_immersive"

                    visible: window.readingBook
                    opacity: readerScreen.chromeShown
                    iconName: "fullscreen"
                    shortcutKey: "F11"
                    text: qsTr("Hide controls (Ctrl+F11)")

                    onClicked: readerScreen.toggleImmersive()
                }
            }

            Item
            {
                id: pages

                Layout.fillWidth: true
                Layout.fillHeight: true
                opacity: window.activeMorph.morphing ? 1 - window.activeMorph.shellContentHidden : 1
                clip: window.sliding

                Rectangle
                {
                    anchors.fill: parent
                    color: AppTheme.colourPanel
                }

                DesktopTodayScreen
                {
                    id: todayScreen

                    anchors.fill: parent
                    visible: window.pageShown("today")
                    transform: Translate { x: window.pageOffset("today") }

                    store: window.store
                    training: training
                    review: review
                    sentences: sentenceTraining

                    onLearnRequested: window.startFromTile("learn", window.startTraining)
                    onReviewRequested: window.startFromTile("review", window.startReview)
                    onFormsTrainingRequested: function(kind) { window.startFromTile(kind, function() { window.startFormsTraining(kind) }) }
                    onSentencesRequested: sentenceDialog.openFor()
                    onPracticeRequested: practiceDialog.openFor()
                    onWordListRequested: function(source) { window.openWordList(source) }
                    onMessageRequested: function(message) { toast.show(message) }
                }

                AppHeartbeat {}

                DesktopVocabularyScreen
                {
                    id: vocabularyScreen

                    anchors.fill: parent
                    visible: window.pageShown("vocabulary")
                    transform: Translate { x: window.pageOffset("vocabulary") }

                    store: window.store

                    onDictionaryRequested: function(word, partOfSpeech) { window.openDictionary(word, partOfSpeech) }
                    onRemoveCardRequested: function(cardId, word) { window.askToRemoveCard(cardId, word) }

                    onCardRemoved: function(cardId)
                    {
                        training.removeCard(cardId)
                        review.removeCard(cardId)
                    }
                }

                AppHeartbeat {}

                DesktopDecksScreen
                {
                    id: decksScreen

                    anchors.fill: parent
                    visible: window.pageShown("decks")
                    transform: Translate { x: window.pageOffset("decks") }

                    store: window.store

                    onDictionaryRequested: function(word, partOfSpeech) { window.openDictionary(word, partOfSpeech) }
                    onRemoveCardRequested: function(cardId, word) { window.askToRemoveCard(cardId, word) }
                    onTrainRequested: function(deckId, source) { window.startFromDeck(deckId, source) }

                    onCardRemoved: function(cardId)
                    {
                        training.removeCard(cardId)
                        review.removeCard(cardId)
                    }
                }

                AppHeartbeat {}

                DesktopSettingsScreen
                {
                    id: settingsScreen

                    anchors.fill: parent
                    visible: window.pageShown("settings")
                    transform: Translate { x: window.pageOffset("settings") }

                    store: window.store
                    triageController: triage

                    onPlacementRequested:
                    {
                        placementScreen.retake = true
                        window.enterSession("placement")
                    }
                }

                AppHeartbeat {}

                DesktopReaderScreen
                {
                    id: readerScreen

                    anchors.fill: parent
                    visible: window.pageShown("reader")
                    transform: Translate { x: window.pageOffset("reader") }

                    store: window.store

                    onAddWordRequested: function(word, translation, partOfSpeech, hint) { window.addWord(word, translation, partOfSpeech, "", hint) }
                    onLookUpRequested: function(word) { window.lookUpWord(word) }
                    onMessageRequested: function(message) { toast.show(message) }
                    onCrumbsChanging: topBar.revealCrumbs()
                }

                AppHeartbeat {}

                DesktopWordsScreen
                {
                    id: wordsScreen

                    anchors.fill: parent
                    visible: window.pageShown("words")
                    transform: Translate { x: window.pageOffset("words") }

                    store: window.store

                    onAddWordRequested: function(word, translation, partOfSpeech, meaningGist) { window.addWord(word, translation, partOfSpeech, meaningGist) }

                    onEditWordRequested: function(cardId, word, translation, gender, partOfSpeech, box)
                    {
                        window.editWord(vocabularyScreen.vocabulary, cardId, word, translation, gender, partOfSpeech, box)
                    }
                }
            }
        }
    }

    AppHeartbeat {}

    DesktopSessionScreen
    {
        id: sessionScreen

        anchors.fill: parent
        visible: window.inSessionScreen
        focus: window.inSessionScreen

        store: window.store
        session: window.inSessionScreen ? window.activeSession : null
        sessionTitle:
        {
            switch (window.session)
            {
            case "review": return qsTr("Review")
            case "sentences": return qsTr("Sentences")
            case "practice": return qsTr("Practice")
            }

            if (training.formsSession)
            {
                return qsTr("Forms training")
            }

            return qsTr("Learning")
        }
        wordCount:
        {
            switch (window.session)
            {
            case "review": return review.cardCount
            case "sentences": return sentenceTraining.sessionSentenceCount
            case "practice": return practice.activeController ? practice.activeController.currentSessionCount : 0
            }

            return training.sessionWordCount
        }
        elapsedSeconds:
        {
            switch (window.session)
            {
            case "review": return review.elapsedSeconds
            case "sentences": return sentenceTraining.elapsedSeconds
            case "practice": return practice.activeController ? practice.activeController.elapsedSeconds : 0
            }

            return training.elapsedSeconds
        }
        completeHeadline:
        {
            switch (window.session)
            {
            case "review": return qsTr("Review complete")
            case "sentences": return qsTr("Sentences complete")
            case "practice": return qsTr("Practice complete")
            }

            return qsTr("Training complete")
        }
        countLabel: window.session === "sentences" ? qsTr("sentences practised") : qsTr("words trained")

        suspended: wordDialog.visible

        onLeaveRequested: window.leaveSession()
        onEndRequested: window.endSession()
        onFinishedClicked:
        {
            if (window.session === "sentences")
            {
                sentenceTraining.endSession()
            }

            window.leaveSession()
        }
        onAddWordRequested: function(word, translation, partOfSpeech, hint) { window.addWord(word, translation, partOfSpeech, "", hint) }
        onEditCardRequested: function(controller)
        {
            const card = controller ? controller.currentCard : null

            if (!card || !card.id)
            {
                return
            }

            window.editWord(controller, card.id, card.targetWord, card.translation, card.gender, card.partOfSpeech, card.box)
        }
    }

    AppHeartbeat {}

    DesktopTriageScreen
    {
        id: triageScreen

        anchors.fill: parent
        visible: !window.onboarding && window.session === "triage"
        focus: visible

        store: window.store
        controller: triage

        onDictionaryRequested: function(word, partOfSpeech) { window.openDictionaryFromSession(word, partOfSpeech) }

        onLeaveRequested: window.leaveSession()
    }

    DesktopOnboardingPlacement
    {
        id: placementScreen

        anchors.fill: parent
        visible: !window.onboarding && window.session === "placement"
        focus: visible

        controller: placementTest
        targetLanguage: window.store.targetLanguageCode
        standalone: true

        onFinished:
        {
            placementScreen.retake = false
            window.leaveSession()
        }
    }

    DesktopOnboardingScreen
    {
        anchors.fill: parent
        visible: window.onboarding
        focus: visible

        store: window.store
        placement: placementTest

        onFinished:
        {
            AppOnboarding.finish()
            window.navigate("today")
        }
    }

    DesktopCardDetailsDialog
    {
        id: wordDialog

        store: window.store

        onClosed: if (window.focusMode) sessionScreen.focusExercise()
        onDictionaryRequested: function(word, partOfSpeech) { window.openDictionary(word, partOfSpeech) }

        onAdded:
        {
            if (window.screen === "vocabulary")
            {
                vocabularyScreen.refresh()
            }
            else if (window.screen === "decks")
            {
                decksScreen.refresh()
            }
            else if (window.screen === "reader")
            {
                readerScreen.refresh()
            }
            else if (window.screen === "words")
            {
                wordsScreen.refresh()
            }
        }
    }

    DesktopConfirmDialog
    {
        id: confirmDialog
    }

    DesktopTenseDialog
    {
        id: tenseDialog

        property string trainingId: ""
        property string origin: ""

        onChosen: function(tense)
        {
            window.startFrom(tenseDialog.origin, tenseDialog.background, tenseDialog.background,
                             function() { window.startTenseTraining(tenseDialog.trainingId, tense) })
        }
    }

    DesktopSentenceDialog
    {
        id: sentenceDialog

        controller: sentenceTraining

        onExerciseChosen: function(exercise)
        {
            window.startFrom("sentences", sentenceDialog.background, sentenceDialog.background,
                             function() { window.startSentences(exercise) })
        }
    }

    DesktopPracticeDialog
    {
        id: practiceDialog

        vocabulary: vocabularyScreen.vocabulary

        onExerciseChosen: function(exercise, cardCount, repetitionCount, filterKeys, reverse) { window.startPractice(exercise, cardCount, repetitionCount, filterKeys, reverse) }
    }

    DesktopToast
    {
        id: toast

        z: 100
    }

    DesktopWelcomeScreen
    {
        id: welcome

        objectName: "welcome"

        anchors.fill: parent
        z: 90
        shown: false

        name: AppSyncController.accountName.split(" ")[0] || ""
        reviewCount: todayScreen.reviewCount
        newCount: todayScreen.wordsLeftForGoal
        reviewProgress: todayScreen.reviewRatio
        learnProgress: todayScreen.goalRatio
        doneToday: todayScreen.ringDone
        dailyGoal: todayScreen.ringGoal
        showReviewRing: todayScreen.hasReviewGoal
        showLearnRing: todayScreen.hasLearnGoal
        target: todayScreen.goalRing

        onFinished: console.info("Welcome screen finished")
    }

    Binding
    {
        target: AppSyncController
        property: "mergesHeld"
        value: welcome.shown && window.visible
    }

    Shortcut
    {
        sequences: [StandardKey.Find]
        enabled: !window.focusMode
        onActivated: window.focusSearch()
    }

    Shortcut
    {
        sequences: [StandardKey.New]
        enabled: !window.focusMode
        onActivated: window.createItem()
    }

    Shortcut
    {
        id: languageSwitchShortcut

        sequence: "Ctrl+0"
        enabled: !window.focusMode && !(window.screen === "reader" && readerScreen.reading)
        onActivated: topBar.openLanguageSwitch()
    }

    Shortcut
    {
        sequence: "Ctrl+T"
        enabled: !window.focusMode && window.readingBook && !window.immersive
        onActivated: readerScreen.toggleContents()
    }

    Shortcut
    {
        sequence: "Ctrl+F11"
        enabled: !window.focusMode && window.readingBook
        onActivated: readerScreen.toggleImmersive()
    }

    function toggleFullScreen()
    {
        if (window.visibility === Window.FullScreen)
        {
            console.info("Leaving full screen")
            window.visibility = window.visibilityBeforeFullScreen

            return
        }

        console.info("Entering full screen")
        window.visibilityBeforeFullScreen = window.visibility
        window.visibility = Window.FullScreen
    }

    Shortcut
    {
        sequence: "F11"
        onActivated: window.toggleFullScreen()
    }

    function navigateToPlace(index)
    {
        if (index >= topBar.destinations.length)
        {
            return
        }

        window.navigate(topBar.destinations[index].key)
    }

    Shortcut
    {
        sequence: "Ctrl+1"
        enabled: !window.focusMode
        onActivated: window.navigateToPlace(0)
    }

    Shortcut
    {
        sequence: "Ctrl+2"
        enabled: !window.focusMode
        onActivated: window.navigateToPlace(1)
    }

    Shortcut
    {
        sequence: "Ctrl+3"
        enabled: !window.focusMode
        onActivated: window.navigateToPlace(2)
    }

    Shortcut
    {
        sequence: "Ctrl+4"
        enabled: !window.focusMode
        onActivated: window.navigateToPlace(3)
    }

    Shortcut
    {
        sequence: "Ctrl+5"
        enabled: !window.focusMode
        onActivated: window.navigateToPlace(4)
    }

    Shortcut
    {
        sequences: ["Ctrl+6", "Ctrl+,"]
        enabled: !window.focusMode
        onActivated: window.navigate("settings")
    }

    property int walksRequested: (screenWalk.requested ? 1 : 0) + (navigationWalk.requested ? 1 : 0)
    property int walksFinished: 0

    onWalksFinishedChanged:
    {
        if (window.walksRequested > 0 && window.walksFinished >= window.walksRequested)
        {
            Qt.quit()
        }
    }

    ScreenWalk
    {
        id: screenWalk

        window: window

        onFinished: window.walksFinished = window.walksFinished + 1
    }

    DesktopNavigationWalk
    {
        id: navigationWalk

        window: window

        onFinished: window.walksFinished = window.walksFinished + 1
    }

    DictionaryBenchmark
    {
        id: dictionaryBenchmark

        view: wordsScreen
    }

    Connections
    {
        target: AppSyncController

        function onDataMerged()
        {
            window.refreshCurrentScreen()

            AppOnboarding.refresh()
        }

        function onLocalDataChanged(replaced)
        {
            if (training.hasActiveSession)
            {
                training.endSession()
            }

            if (review.hasActiveSession)
            {
                review.endSession()
            }

            sentenceTraining.forgetSessions()

            window.session = ""
            window.refreshCurrentScreen()

            AppOnboarding.refresh()
        }
    }

    Connections
    {
        target: window.store

        function onDebugTimeChanged()
        {
            window.refreshCurrentScreen()
        }

        function onLanguagesChanged()
        {
            if (training.hasActiveSession)
            {
                training.endSession()
            }

            sentenceTraining.forgetSessions()

            window.session = ""
            window.suspendedSession = ""
            decksScreen.closeDeck()
            wordsScreen.selectedIndex = -1
            window.refreshCurrentScreen()

            if (window.onboarding)
            {
                return
            }

            if (placementTest.isOffered(window.store.targetLanguageCode, ""))
            {
                window.enterSession("placement")

                return
            }

            AppOnboarding.markLanguageSetUp(window.store.targetLanguageCode)
        }
    }

    Connections
    {
        target: AppLocalization

        function onLanguageChanged()
        {
            window.refreshCurrentScreen()
        }
    }

    Connections
    {
        target: AppSyncController

        function onExportFinished(success, message)
        {
            if (message.length > 0)
            {
                toast.show(message)
            }
        }

        function onImportFinished(success, message)
        {
            toast.show(message)
        }

        function onLanguageDeleted(success, message)
        {
            toast.show(message)
        }
    }

    Connections
    {
        target: AppPackageDownloader

        function onPackageInstalled(id)
        {
            toast.show(qsTr("%1 is ready.").arg(AppCatalog.describePackage(id)))
        }

        function onPackageFailed(id, error)
        {
            toast.show(qsTr("%1 could not be installed.").arg(AppCatalog.describePackage(id)))
        }
    }
}
