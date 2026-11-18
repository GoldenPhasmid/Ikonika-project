import QtQuick
import QtQuick.Controls
import QtQuick.Window

Page
{
    id: window

    anchors.fill: parent
    focus: true

    readonly property bool readingBook: window.screen === "reader" && readerMobile.reading

    topPadding: window.readingBook ? 0 : window.SafeArea.margins.top
    bottomPadding: window.readingBook ? 0 : window.SafeArea.margins.bottom
    leftPadding: window.readingBook ? 0 : window.SafeArea.margins.left
    rightPadding: window.readingBook ? 0 : window.SafeArea.margins.right

    /** The dictionary update is offered once a run, so a catalog reread does not raise it again. */
    property bool offeredDictionaryUpdate: false

    palette.window: AppTheme.colourPanel
    palette.windowText: AppTheme.colourText
    palette.base: AppTheme.colourPanel
    palette.text: AppTheme.colourText
    palette.button: AppTheme.colourBorder
    palette.buttonText: AppTheme.colourText
    palette.highlight: AppTheme.colourAccent
    palette.highlightedText: AppTheme.colourAccentText
    palette.mid: AppTheme.colourBorder
    palette.dark: AppTheme.colourBorder
    palette.light: AppTheme.colourMuted

    required property AppControllers controllers
    property var layoutState: ({})

    readonly property DailySessionController daily: window.controllers.daily
    readonly property TriageController triage: window.controllers.triage
    readonly property ExerciseLauncherController exerciseLauncher: window.controllers.launcher
    readonly property SentenceTrainingController sentenceTraining: window.controllers.sentenceTraining
    readonly property DiscoveryController discovery: window.controllers.discovery
    readonly property PlacementTestController placementTest: window.controllers.placementTest
    readonly property IntensiveLearningController training: window.controllers.training
    readonly property RepetitionController review: window.controllers.review

    Binding
    {
        target: window.Window.window
        when: window.Window.window !== null
        property: "color"
        value: AppTheme.colourPanel
    }

    function handleClose()
    {
        if (Qt.platform.os === "android")
        {
            if (!window.androidBack())
            {
                AppReminder.moveToBackground()
            }

            return false
        }

        if (!AppReminder.trayAvailable)
        {
            return true
        }

        window.Window.window.hide()

        return false
    }

    function androidBack()
    {
        if (lookupMobile.shown || lookupMobile.pending)
        {
            lookupMobile.close()
            return true
        }

        if (window.screen === "onboarding")
        {
            const placementIntro = window.onboardingStep !== "placement"
                || (onboardingPlacement.item && onboardingPlacement.item.phase === "intro")

            if (AppOnboarding.canGoBack && placementIntro)
            {
                AppOnboarding.back()
            }

            return true
        }

        if (window.screen === "reader" && readerMobile.dismissSheet())
        {
            return true
        }

        if (window.screen === "reader" && readerMobile.reading)
        {
            readerMobile.closeBook()

            return true
        }

        if (window.screen === "settings" && settingsMobile.item && settingsMobile.item.closeCategory())
        {
            return true
        }

        if (window.screen === "dictionaryword" && dictionaryWordMobile.item
            && dictionaryWordMobile.item.closeConjugation())
        {
            return true
        }

        return window.popScreen()
    }

    property var screenStack: [screenWalk.requested
        ? screenWalk.screens[0]
        : (AppOnboarding.required ? "onboarding" : "today")]

    readonly property string screen: window.screenStack.length > 0
        ? window.screenStack[window.screenStack.length - 1] : ""

    property int transitionDirection: 0

    function pushScreen(name)
    {
        window.transitionDirection = 1
        window.screenStack = window.screenStack.concat([name])
    }

    function showScreen(name)
    {
        if (window.screen !== name)
        {
            window.pushScreen(name)
        }
    }

    function replaceScreen(name)
    {
        window.transitionDirection = 0
        window.screenStack = window.screenStack.slice(0, -1).concat([name])
    }

    function resetTo(name)
    {
        if (name === "vocabulary")
        {
            window.openVocabulary("")

            return
        }

        window.transitionDirection = 0
        window.screenStack = [name]
    }

    function screenBelow()
    {
        return window.screenStack.length >= 2
            ? window.screenStack[window.screenStack.length - 2] : ""
    }

    function openLookup(word, sentence)
    {
        Qt.inputMethod.hide()
        lookupMobile.openFor(word, false, sentence)
    }

    function openPendingText()
    {
        if (AppOnboarding.required)
        {
            return
        }

        const text = AppTextIntake.takePending()
        if (text.length === 0)
        {
            return
        }

        welcome.shown = false
        window.openLookup(text, "")
    }

    function captureLayoutState()
    {
        const tab = window.screenStack.length > 0 ? window.screenStack[0] : ""

        if (AppOnboarding.required || tab === "onboarding")
        {
            return {}
        }

        const sessions = {
            intensive: "training",
            repetition: "review",
            discovery: "discovery",
            sentencesession: "sentences",
            triage: "triage",
            placement: "placement",
            flashcards: "practice",
            flip: "practice",
            correctincorrect: "practice",
            choice: "practice",
            written: "practice",
            audio: "practice",
            speedmatch: "practice"
        }
        const open = window.screenStack.filter(Screen => sessions[Screen] !== undefined)

        return {
            place: tab === "dictionary" ? "words" : tab,
            session: open.length > 0 ? sessions[open[open.length - 1]] : ""
        }
    }

    function restoreLayoutState(state)
    {
        if (AppOnboarding.required || !state.place)
        {
            return
        }

        console.info("Layout switch: restoring", state.place, state.session)

        welcome.shown = false
        if (state.place === "words")
        {
            window.dictionaryAlreadyFresh = true
            dictionaryMobile.active = true
        }

        window.resetTo(state.place === "words" ? "dictionary" : state.place)

        const exercise = window.exerciseLauncher.exercise
        const screens = {
            training: "intensive",
            review: "repetition",
            discovery: "discovery",
            sentences: "sentencesession",
            triage: "triage",
            placement: "placement",
            practice: exercise === "gap" ? "written" : exercise
        }
        const screen = screens[state.session]

        if (!screen)
        {
            return
        }

        if (screen === "triage")
        {
            triageMobile.active = true
        }

        window.pushScreen(screen)
    }

    function popScreen()
    {
        if (window.screenStack.length > 1)
        {
            window.transitionDirection = -1
            window.screenStack = window.screenStack.slice(0, -1)

            return true
        }

        if (window.screen === "today")
        {
            return false
        }

        window.resetTo("today")

        return true
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

    NavigationWalk
    {
        id: navigationWalk

        window: window

        onFinished: window.walksFinished = window.walksFinished + 1
    }

    property bool vocabularyAlreadyFresh: false

    property string previousScreen: ""
    property bool dictionaryAlreadyFresh: false
    property bool deckAlreadyFresh: false

    onScreenChanged:
    {
        window.contentItem.forceActiveFocus()

        window.refreshCurrentScreen()
        window.focusTypedScreen()

        window.previousScreen = window.screen
    }

    function focusTypedScreen()
    {
        switch (window.screen)
        {
        case "written": if (writtenMobile.item) writtenMobile.item.focusAnswer(); return
        case "audio": if (audioMobile.item) audioMobile.item.focusAnswer(); return
        }
    }

    function openVocabulary(filterKey)
    {
        vocabularyMobile.resetView()

        if (filterKey.length > 0)
        {
            vocabularyMobile.vocabulary.filterKeys = [filterKey]
        }

        window.transitionDirection = 0
        window.screenStack = ["vocabulary"]
    }

    readonly property var refreshableScreens: ({
        "today": todayMobile,
        "vocabulary": vocabularyMobile,
        "decks": decksMobile,
        "deck": deckMobile,
        "dictionary": dictionaryMobile,
        "wordlist": wordListMobile,
        "reader": readerMobile
    })

    readonly property var testHooks: ({
        todayMobile: todayMobile,
        vocabularyMobile: vocabularyMobile,
        flashcardsMobile: flashcardsMobile,
        triageMobile: triageMobile,
        intensiveMobile: intensiveMobile,
        repetitionMobile: repetitionMobile,
        dictionaryMobile: dictionaryMobile,
        settingsMobile: settingsMobile,
        dictionaryWordMobile: dictionaryWordMobile,
        wordListMobile: wordListMobile,
        practiceMobile: practiceMobile,
        addWordDialogMobile: addWordDialogMobile,
        dictionaryAddWordDialogMobile: dictionaryAddWordDialogMobile
    })

    function refreshCurrentScreen()
    {
        window.store.clearLastError()

        window.daily.refresh()

        if (window.screen === "vocabulary" && window.vocabularyAlreadyFresh)
        {
            window.vocabularyAlreadyFresh = false

            return
        }

        if (window.screen === "dictionary" && window.dictionaryAlreadyFresh)
        {
            window.dictionaryAlreadyFresh = false

            return
        }

        if (window.screen === "deck" && window.deckAlreadyFresh)
        {
            window.deckAlreadyFresh = false

            return
        }

        if (window.screen === "dictionary" && window.previousScreen !== "dictionaryword"
            && window.previousScreen !== "dictionaryaddword")
        {
            dictionaryMobile.reset()

            return
        }

        const target = window.refreshableScreens[window.screen]

        if (target && typeof target.refresh === "function")
        {
            target.refresh()
        }
    }

    Component.onCompleted:
    {
        if (!window.layoutState.place && window.screen === "today")
        {
            if (repetitionMobile.hasActiveSession)
            {
                window.pushScreen("repetition")
            }
            else
            {
                const sessionKey = intensiveMobile.controller.lastActiveKey()

                if (sessionKey.length > 0 && intensiveMobile.resume(sessionKey))
                {
                    window.pushScreen("intensive")
                }
            }
        }

        window.refreshCurrentScreen()
        screenWalk.start()
        navigationWalk.start()

        const snapshotDirectory = snapshotProbe.environmentVariable("IKONIKA_SNAPSHOT_DIR")

        if (snapshotDirectory.length > 0)
        {
            snapshotProbe.serveSnapshots(window.contentItem, snapshotDirectory)
        }

        if (window.screen === "today" && window.walksRequested === 0)
        {
            welcome.shown = true
            welcome.forceActiveFocus()
        }

        if (window.layoutState.place)
        {
            window.restoreLayoutState(window.layoutState)
        }

        Qt.callLater(window.openPendingText)
    }

    readonly property SettingsStore store: SettingsStore {}

    function lookUpWord(term)
    {
        if (window.screen !== "dictionary")
        {
            window.dictionaryAlreadyFresh = true
        }

        dictionaryMobile.searchTerm(term)

        window.resetTo("dictionary")
    }

    property bool placementRetake: false

    function startTraining()
    {
        if (intensiveMobile.resume(intensiveMobile.controller.vocabularyKey))
        {
            window.pushScreen("intensive")
            return
        }

        if (window.daily.shortfall() > 0)
        {
            console.info("Train: not enough words in My Vocabulary, opening Discover")
            window.startDiscovery()

            return
        }

        if (intensiveMobile.start())
        {
            window.pushScreen("intensive")
        }
    }

    function editSessionCard(exercise)
    {
        const card = exercise ? exercise.currentCard : null

        if (!card || !card.id)
        {
            console.warn("Edit card asked with no card on screen")

            return
        }

        dictionaryAddWordDialogMobile.openForCard(exercise, card.id, card.targetWord, card.translation,
                                                  card.gender, card.partOfSpeech, card.box)
        window.pushScreen("dictionaryaddword")
    }

    property string tenseSheetTraining: ""
    property var tenseSheetTenses: []

    function startFormsTraining(training)
    {
        const trainingId = window.daily.formsTrainingId(training)

        if (trainingId.length === 0)
        {
            return
        }

        if (intensiveMobile.resume(trainingId))
        {
            window.pushScreen("intensive")

            return
        }

        const tenses = window.daily.formsTenses(trainingId)

        if (tenses.length > 1)
        {
            window.tenseSheetTenses = tenses
            window.tenseSheetTraining = trainingId

            return
        }

        window.startTenseTraining(trainingId, "present")
    }

    function startTenseTraining(trainingId, tense)
    {
        const cards = tense === "present" ? window.daily.collectFormsCards(trainingId)
                                          : window.daily.collectTenseCards(trainingId, tense)

        if (intensiveMobile.startWithCards(trainingId, cards))
        {
            window.pushScreen("intensive")
        }
    }

    function startCollectedTraining()
    {
        if (intensiveMobile.start())
        {
            window.replaceScreen("intensive")

            return
        }

        window.popScreen()
    }

    readonly property var shellScreens:
        ["today", "vocabulary", "decks", "deck", "dictionary", "wordlist", "settings", "reader"]

    readonly property bool inShell: window.shellScreens.indexOf(window.screen) >= 0

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
            if (intensiveMobile.hasActiveSession)
            {
                intensiveMobile.controller.endSession()
            }

            if (repetitionMobile.hasActiveSession)
            {
                repetitionMobile.controller.endSession()
            }

            if (window.screen === "intensive" || window.screen === "repetition")
            {
                window.resetTo("today")
            }

            window.refreshCurrentScreen()

            AppOnboarding.refresh()
        }
    }

    Connections
    {
        target: AppOnboarding

        function onRequiredChanged()
        {
            if (!AppOnboarding.required)
            {
                if (window.screen === "onboarding")
                {
                    window.resetTo("today")
                }

                Qt.callLater(window.openPendingText)

                return
            }

            window.resetTo("onboarding")
        }
    }

    Connections
    {
        target: window.store

        function onDebugTimeChanged()
        {
            window.daily.refresh()
            todayMobile.refresh()
            vocabularyMobile.refresh()
        }

        function onLanguagesChanged()
        {
            if (intensiveMobile.hasActiveSession)
            {
                intensiveMobile.controller.endSession()
            }

            window.sentenceTraining.forgetSessions()

            if (window.screen === "intensive" || window.screen === "sentencesession" || window.screen === "discovery"
                || window.screen === "discoverpick")
            {
                window.resetTo("today")
            }
            else
            {
                window.refreshCurrentScreen()
            }

            if (window.screen === "onboarding" || window.screen === "placement")
            {
                return
            }

            if (window.placementTest.isOffered(window.store.targetLanguageCode, ""))
            {
                window.placementRetake = false
                window.pushScreen("placement")

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

    function startExercise(exercise, reverse, cardCount, repetitionCount, filterKeys)
    {
        if (window.exerciseLauncher.startExercise(exercise, !!reverse,
            cardCount === undefined ? -1 : cardCount,
            repetitionCount === undefined ? -1 : repetitionCount,
            filterKeys === undefined ? [] : filterKeys))
        {
            window.pushScreen(exercise === "gap" ? "written" : exercise)
        }
    }

    MobileTodayScreen
    {
        id: todayMobile

        daily: window.daily

        anchors.fill: parent
        visible: false

        store: window.store
        repetition: repetitionMobile
        intensive: intensiveMobile
        sentences: window.sentenceTraining
        discovery: window.discovery

        onIntensiveRequested: window.startTraining()
        onDiscoverRequested: window.startDiscovery()

        onRepetitionRequested:
        {
            if (repetitionMobile.startDueToday())
            {
                window.pushScreen("repetition")
            }
        }

        onSentencesRequested: window.pushScreen("sentences")
        onPracticeRequested: window.pushScreen("practice")
        onFormsTrainingRequested: function(training) { window.startFormsTraining(training) }

        onDictionaryRequested: window.pushScreen("dictionary")
        onSettingsRequested: window.pushScreen("settings")

        onWordListRequested: function(source)
        {
            wordListMobile.open(source)
            window.pushScreen("wordlist")
        }

        onWordLookupRequested: function(term)
        {
            window.lookUpWord(term)
        }

        onTabSelected: function(key)
        {
            window.resetTo(key)
        }
    }

    Component
    {
        id: wordListMobileComponent

        MobileWordListScreen
        {
            controller: window.controllers.wordList

            anchors.fill: parent

            store: window.store

            onDone: window.popScreen()

            onWordOpened: function(headword, partOfSpeech)
            {
                dictionaryWordMobile.open(headword, partOfSpeech)
                window.pushScreen("dictionaryword")
            }

            onTabSelected: function(key)
            {
                window.resetTo(key)
            }
        }
    }

    Loader
    {
        id: wordListMobile

        anchors.fill: parent
        visible: false
        active: false
        asynchronous: false
        sourceComponent: wordListMobileComponent

        function open(source)
        {
            wordListMobile.active = true
            wordListMobile.item.open(source)
        }

        function refresh()
        {
            if (wordListMobile.item)
            {
                wordListMobile.item.refresh()
            }
        }
    }

    Component
    {
        id: settingsMobileComponent

        MobileSettingsScreen
        {
            anchors.fill: parent

            store: window.store
            triageController: window.triage

            onDone: window.popScreen()

            onTabSelected: function(key)
            {
                window.resetTo(key)
            }

            onPlacementRequested:
            {
                window.placementRetake = true
                window.pushScreen("placement")
            }
        }
    }

    MobileScreenLoader
    {
        id: settingsMobile

        anchors.fill: parent
        visible: false
        active: false
        asynchronous: true
        sourceComponent: settingsMobileComponent

        onVisibleChanged: if (visible) active = true

        function refresh()
        {
            if (settingsMobile.item)
            {
                settingsMobile.item.refresh()
            }
        }
    }

    Component
    {
        id: dictionaryMobileComponent

        MobileDictionaryScreen
        {
            controller: window.controllers.dictionary("words")

            anchors.fill: parent

            store: window.store

            onDone: window.popScreen()

            onAddWordRequested: function(word, translation, partOfSpeech, hint)
            {
                dictionaryAddWordDialogMobile.openWith(word, translation, partOfSpeech, "", hint)
                window.pushScreen("dictionaryaddword")
            }

            onEntryOpened: function(headword, partOfSpeech, searchText)
            {
                dictionaryWordMobile.open(headword, partOfSpeech, searchText)
                window.pushScreen("dictionaryword")
            }

            onTabSelected: function(key)
            {
                window.resetTo(key)
            }
        }
    }

    Loader
    {
        id: dictionaryMobile

        anchors.fill: parent
        visible: false
        active: false
        asynchronous: false
        sourceComponent: dictionaryMobileComponent

        function reset()
        {
            dictionaryMobile.active = true
            dictionaryMobile.item.reset()
        }

        function searchTerm(term)
        {
            dictionaryMobile.active = true
            dictionaryMobile.item.searchTerm(term)
        }

        function refresh()
        {
            dictionaryMobile.refreshLearnedMarks()
        }

        function refreshLearnedMarks()
        {
            if (dictionaryMobile.item)
            {
                dictionaryMobile.item.refreshLearnedMarks()
            }
        }
    }

    function openWidgetSession()
    {
        if (AppOnboarding.required)
        {
            return
        }

        const session = AppHomeWidget.takePendingSession()

        if (session === "review")
        {
            window.resetTo("today")

            if (repetitionMobile.startDueToday())
            {
                window.pushScreen("repetition")
            }
        }
        else if (session === "learn")
        {
            window.resetTo("today")
            todayMobile.startFromPool()
        }
    }

    Connections
    {
        target: AppHomeWidget

        function onSessionRequested()
        {
            window.openWidgetSession()
        }
    }

    Connections
    {
        target: AppTextIntake

        function onTextArrived()
        {
            window.openPendingText()
        }
    }

    Connections
    {
        target: AppReaderIntake

        function onBookArrived()
        {
            if (AppOnboarding.required)
            {
                return
            }

            const book = AppReaderIntake.takePending()

            if (!book.path)
            {
                return
            }

            readerMobile.pendingBook = book
            window.resetTo("reader")
            readerMobile.active = true
            readerMobile.openPendingBook()
        }
    }

    Component
    {
        id: readerMobileComponent

        MobileReaderScreen
        {
            lookupDictionary: window.controllers.dictionary("readerLookup")

            reader: window.controllers.reader
            stageCount: window.store.stageCount

            anchors.fill: parent

            onAddWordRequested: function(word, translation, partOfSpeech, hint)
            {
                dictionaryAddWordDialogMobile.openWith(word, translation, partOfSpeech, "", hint)
                window.pushScreen("dictionaryaddword")
            }

            onEntryOpened: function(headword, partOfSpeech, hint)
            {
                dictionaryWordMobile.open(headword, partOfSpeech, "", hint)
                window.pushScreen("dictionaryword")
            }

            onTabSelected: function(key)
            {
                window.resetTo(key)
            }
        }
    }

    Connections
    {
        target: vocabularyMobile.vocabulary

        function onCardAdded(cardId)
        {
            if (window.screenStack[0] === "reader" && readerMobile.item)
            {
                readerMobile.item.recordAddedCard(cardId)
            }
        }
    }

    MobileScreenLoader
    {
        id: readerMobile

        anchors.fill: parent
        visible: false
        active: false
        asynchronous: true
        sourceComponent: readerMobileComponent

        onVisibleChanged: if (visible) active = true

        readonly property bool reading: readerMobile.item ? readerMobile.item.reading : false

        property var pendingBook: null

        onLoaded: readerMobile.openPendingBook()

        function openPendingBook()
        {
            if (!readerMobile.item || readerMobile.pendingBook === null)
            {
                return
            }

            const book = readerMobile.pendingBook
            readerMobile.pendingBook = null
            readerMobile.item.openExternal(book.path, book.name)
        }

        function dismissSheet()
        {
            return readerMobile.item ? readerMobile.item.dismissSheet() : false
        }

        function closeBook()
        {
            if (readerMobile.item)
            {
                readerMobile.item.closeBook()
            }
        }

        function refresh()
        {
            if (readerMobile.item)
            {
                readerMobile.item.refresh()
            }
        }
    }

    Component
    {
        id: practiceMobileComponent

        MobilePracticeScreen
        {
            anchors.fill: parent

            vocabulary: vocabularyMobile.vocabulary

            onDone: window.popScreen()

            onExerciseRequested: function(exercise, reverse, cardCount, repetitionCount, filterKeys)
            {
                window.startExercise(exercise, reverse, cardCount, repetitionCount, filterKeys)
            }

            onTabSelected: function(key)
            {
                window.resetTo(key)
            }
        }
    }

    Loader
    {
        id: practiceMobile

        anchors.fill: parent
        visible: false
        active: false
        asynchronous: false
        sourceComponent: practiceMobileComponent

        onVisibleChanged:
        {
            if (practiceMobile.visible)
            {
                practiceMobile.active = true
            }
        }
    }

    Component
    {
        id: dictionaryWordMobileComponent

        MobileDictionaryWordScreen
        {
            controller: window.controllers.dictionary("wordEntry")

            anchors.fill: parent

            stageCount: window.store.stageCount
            onDone: window.popScreen()

            onAddWordRequested: function(word, translation, partOfSpeech, meaningGist)
            {
                dictionaryAddWordDialogMobile.openWith(word, translation, partOfSpeech, meaningGist, dictionaryWordMobile.sourceHint)
                window.pushScreen("dictionaryaddword")
            }

            onEditWordRequested: function(cardId, word, translation, gender, partOfSpeech, box)
            {
                dictionaryAddWordDialogMobile.openForEdit(
                    cardId, word, translation, gender, partOfSpeech, box)
                window.pushScreen("dictionaryaddword")
            }

            onTabSelected: function(key)
            {
                window.resetTo(key)
            }

            onSearchRequested: window.lookUpWord("")
            onWordTapped: function(word, sentence) { window.openLookup(word, sentence) }
        }
    }

    Loader
    {
        id: dictionaryWordMobile

        anchors.fill: parent
        visible: false
        active: false
        asynchronous: false
        sourceComponent: dictionaryWordMobileComponent

        property string sourceHint: ""

        function open(headword, partOfSpeech, searchText, hint)
        {
            dictionaryWordMobile.sourceHint = hint || ""
            dictionaryWordMobile.active = true
            dictionaryWordMobile.item.open(headword, partOfSpeech, searchText || "")
        }

        function refreshLearnedMarks()
        {
            if (dictionaryWordMobile.item)
            {
                dictionaryWordMobile.item.refreshLearnedMarks()
            }
        }
    }

    MobileVocabularyScreen
    {
        id: vocabularyMobile

        controller: window.controllers.vocabulary

        anchors.fill: parent
        visible: false

        store: window.store

        onPracticeRequested: window.pushScreen("practice")

        onAddWordRequested: window.lookUpWord("")
        onSearchRequested: window.lookUpWord("")

        onWordOpened: function(word, partOfSpeech)
        {
            dictionaryWordMobile.open(word, partOfSpeech)
            window.pushScreen("dictionaryword")
        }

        onEditWordRequested: function(cardId, word, translation, gender, partOfSpeech, box)
        {
            addWordDialogMobile.openForEdit(cardId, word, translation, gender, partOfSpeech, box)
            window.pushScreen("addword")
        }

        onCardRemoved: function(cardId)
        {
            intensiveMobile.controller.removeCard(cardId)
            repetitionMobile.controller.removeCard(cardId)
        }

        onTabSelected: function(key)
        {
            window.resetTo(key)
        }
    }

    Component
    {
        id: triageMobileComponent

        MobileTriageScreen
        {
            id: triageScreen

            controller: window.triage

            anchors.fill: parent

            store: window.store

            onDone: window.popScreen()
            onWordTapped: function(word, sentence) { window.openLookup(word, sentence) }

            onCompleted:
            {
                todayMobile.refresh()

                if (triageScreen.controller.deckMode)
                {
                    deckMobile.refresh()
                    decksMobile.refresh()
                    window.popScreen()

                    return
                }

                window.startCollectedTraining()
            }
        }
    }

    MobileDecksScreen
    {
        id: decksMobile

        anchors.fill: parent
        visible: false

        onDeckOpened: function(deckId)
        {
            deckMobile.open(deckId)
            window.pushScreen("deck")
        }

        onTrainRequested: function(deckId) { window.startDeckDiscovery(deckId) }

        onSearchRequested: window.lookUpWord("")
        onTabSelected: function(key) { window.resetTo(key) }
    }

    Component
    {
        id: deckMobileComponent

        MobileVocabularyScreen
        {
            controller: window.controllers.deckVocabulary

            anchors.fill: parent

            store: window.store

            onLearnRequested: function(deckId) { window.startDeckDiscovery(deckId) }

            onBackRequested: window.popScreen()
            onSearchRequested: window.lookUpWord("")

            onWordOpened: function(word, partOfSpeech)
            {
                dictionaryWordMobile.open(word, partOfSpeech)
                window.pushScreen("dictionaryword")
            }

            onEditWordRequested: function(cardId, word, translation, gender, partOfSpeech, box)
            {
                addWordDialogMobile.openForEdit(cardId, word, translation, gender, partOfSpeech, box)
                window.pushScreen("addword")
            }

            onCardRemoved: function(cardId)
            {
                intensiveMobile.controller.removeCard(cardId)
                repetitionMobile.controller.removeCard(cardId)
            }

            onTabSelected: function(key) { window.resetTo(key) }
        }
    }

    Loader
    {
        id: deckMobile

        anchors.fill: parent
        visible: false
        active: false
        asynchronous: false
        sourceComponent: deckMobileComponent

        function open(deckId)
        {
            deckMobile.active = true

            if (deckMobile.item.deckId === deckId)
            {
                deckMobile.item.resetView()

                return
            }

            deckMobile.item.deckId = deckId
            window.deckAlreadyFresh = true
        }

        function refresh()
        {
            if (deckMobile.item)
            {
                deckMobile.item.refresh()
            }
        }
    }

    Loader
    {
        id: triageMobile

        anchors.fill: parent
        visible: false
        active: false
        asynchronous: false
        sourceComponent: triageMobileComponent

        function start(wantedCount)
        {
            triageMobile.active = true

            return triageMobile.item.start(wantedCount)
        }

        function startDeck(deckId)
        {
            triageMobile.active = true

            return triageMobile.item.controller.startDeck(deckId)
        }
    }

    MobileRepetitionScreen
    {
        id: repetitionMobile

        controller: window.review

        anchors.fill: parent
        visible: false

        store: window.store

        onDone:
        {
            AppReminder.syncDailyProgress()
            vocabularyMobile.refresh()
            window.popScreen()
        }

        onEditCardRequested: function(exercise) { window.editSessionCard(exercise) }

        onLookupRequested: function(word, sentence) { window.openLookup(word, sentence) }
    }

    Component
    {
        id: addWordDialogComponent

        MobileCardDetailsScreen
        {
            dictionary: window.controllers.dictionary("addWord")

            anchors.fill: parent

            controller: vocabularyMobile.vocabulary
            store: window.store

            onDone: window.popScreen()
            onAdded: vocabularyMobile.refresh()
            onWordTapped: function(word, sentence) { window.openLookup(word, sentence) }
        }
    }

    Loader
    {
        id: addWordDialogMobile

        anchors.fill: parent
        visible: false
        z: 50
        active: false
        asynchronous: false
        sourceComponent: addWordDialogComponent

        function openForEdit(cardId, word, translation, gender, partOfSpeech, box)
        {
            addWordDialogMobile.active = true
            addWordDialogMobile.item.openForEdit(cardId, word, translation, gender, partOfSpeech, box)
        }
    }


    Component
    {
        id: dictionaryAddWordDialogComponent

        MobileCardDetailsScreen
        {
            dictionary: window.controllers.dictionary("dictionaryAddWord")

            anchors.fill: parent

            store: window.store
            controller: vocabularyMobile.vocabulary

            onDone: window.popScreen()
            onWordTapped: function(word, sentence) { window.openLookup(word, sentence) }

            onAdded:
            {
                vocabularyMobile.refresh()
                dictionaryMobile.refreshLearnedMarks()
                dictionaryWordMobile.refreshLearnedMarks()
            }
        }
    }

    Loader
    {
        id: dictionaryAddWordDialogMobile

        anchors.fill: parent
        visible: false
        z: 50
        active: false
        asynchronous: false
        sourceComponent: dictionaryAddWordDialogComponent

        function openWith(word, translation, partOfSpeech, meaningGist, hint)
        {
            dictionaryAddWordDialogMobile.active = true
            dictionaryAddWordDialogMobile.item.controller = vocabularyMobile.vocabulary
            dictionaryAddWordDialogMobile.item.openWith(word, translation, partOfSpeech || "", meaningGist || "", hint || "")
        }

        function openForCard(exercise, cardId, word, translation, gender, partOfSpeech, box)
        {
            dictionaryAddWordDialogMobile.active = true
            dictionaryAddWordDialogMobile.item.controller = exercise
            dictionaryAddWordDialogMobile.item.openForEdit(
                cardId, word, translation, gender, partOfSpeech, box)
        }

        function openForEdit(cardId, word, translation, gender, partOfSpeech, box)
        {
            dictionaryAddWordDialogMobile.active = true
            dictionaryAddWordDialogMobile.item.controller = vocabularyMobile.vocabulary
            dictionaryAddWordDialogMobile.item.openForEdit(
                cardId, word, translation, gender, partOfSpeech, box)
        }
    }

    Component
    {
        id: flashcardsMobileComponent

        MobileFlashcardScreen
        {
            store: window.store

            Binding on controller
            {
                value: window.exerciseLauncher.activeController
                when: window.exerciseLauncher.activeController instanceof FlashcardController
                restoreMode: Binding.RestoreNone
            }

            onDone:
            {
                AppReminder.syncDailyProgress()
                window.popScreen()
            }

            onWordTapped: function(word, sentence) { window.openLookup(word, sentence) }
        }
    }

    MobileScreenLoader
    {
        id: flashcardsMobile

        anchors.fill: parent
        visible: false
        active: false
        asynchronous: true
        sourceComponent: flashcardsMobileComponent

        onVisibleChanged: if (visible) active = true
    }

    Component
    {
        id: flipMobileComponent

        MobileFlipScreen
        {
            store: window.store

            Binding on controller
            {
                value: window.exerciseLauncher.activeController
                when: window.exerciseLauncher.activeController instanceof FlipRecallController
                restoreMode: Binding.RestoreNone
            }

            onDone:
            {
                AppReminder.syncDailyProgress()
                window.popScreen()
            }

            onWordTapped: function(word, sentence) { window.openLookup(word, sentence) }
        }
    }

    MobileScreenLoader
    {
        id: flipMobile

        anchors.fill: parent
        visible: false
        active: false
        asynchronous: true
        sourceComponent: flipMobileComponent

        onVisibleChanged: if (visible) active = true
    }

    Component
    {
        id: correctIncorrectMobileComponent

        MobileCorrectIncorrectScreen
        {
            store: window.store

            Binding on controller
            {
                value: window.exerciseLauncher.activeController
                when: window.exerciseLauncher.activeController instanceof CorrectIncorrectController
                restoreMode: Binding.RestoreNone
            }

            onDone:
            {
                AppReminder.syncDailyProgress()
                window.popScreen()
            }

            onWordTapped: function(word, sentence) { window.openLookup(word, sentence) }
        }
    }

    MobileScreenLoader
    {
        id: correctIncorrectMobile

        anchors.fill: parent
        visible: false
        active: false
        asynchronous: true
        sourceComponent: correctIncorrectMobileComponent

        onVisibleChanged: if (visible) active = true
    }

    Component
    {
        id: choiceMobileComponent

        MobileMultipleChoiceScreen
        {
            store: window.store

            Binding on controller
            {
                value: window.exerciseLauncher.activeController
                when: window.exerciseLauncher.activeController instanceof MultipleChoiceController
                restoreMode: Binding.RestoreNone
            }

            onDone:
            {
                AppReminder.syncDailyProgress()
                window.popScreen()
            }
        }
    }

    MobileScreenLoader
    {
        id: choiceMobile

        anchors.fill: parent
        visible: false
        active: false
        asynchronous: true
        sourceComponent: choiceMobileComponent

        onVisibleChanged: if (visible) active = true
    }

    Component
    {
        id: writtenMobileComponent

        MobileWrittenProductionScreen
        {
            store: window.store

            Binding on controller
            {
                value: window.exerciseLauncher.activeController
                when: window.exerciseLauncher.activeController instanceof WrittenProductionController
                restoreMode: Binding.RestoreNone
            }

            onDone:
            {
                AppReminder.syncDailyProgress()
                window.popScreen()
            }

            onWordTapped: function(word, sentence) { window.openLookup(word, sentence) }
        }
    }

    MobileScreenLoader
    {
        id: writtenMobile

        anchors.fill: parent
        visible: false
        active: false
        asynchronous: true
        sourceComponent: writtenMobileComponent

        onVisibleChanged:
        {
            if (!visible)
            {
                return
            }

            active = true

            if (item)
            {
                item.focusAnswer()
            }
        }

        onLoaded: if (writtenMobile.visible) item.focusAnswer()
    }

    Component
    {
        id: audioMobileComponent

        MobileAudioTranscriptionScreen
        {
            store: window.store

            Binding on controller
            {
                value: window.exerciseLauncher.activeController
                when: window.exerciseLauncher.activeController instanceof AudioTranscriptionController
                restoreMode: Binding.RestoreNone
            }

            onDone:
            {
                AppReminder.syncDailyProgress()
                window.popScreen()
            }

            onWordTapped: function(word, sentence) { window.openLookup(word, sentence) }
        }
    }

    MobileScreenLoader
    {
        id: audioMobile

        anchors.fill: parent
        visible: false
        active: false
        asynchronous: true
        sourceComponent: audioMobileComponent

        onVisibleChanged:
        {
            if (!visible)
            {
                return
            }

            active = true

            if (item)
            {
                item.focusAnswer()
            }
        }

        onLoaded: if (audioMobile.visible) item.focusAnswer()
    }

    Component
    {
        id: speedmatchMobileComponent

        MobileSpeedMatchScreen
        {
            store: window.store

            Binding on controller
            {
                value: window.exerciseLauncher.activeController
                when: window.exerciseLauncher.activeController instanceof SpeedMatchController
                restoreMode: Binding.RestoreNone
            }

            onDone:
            {
                AppReminder.syncDailyProgress()
                window.popScreen()
            }
        }
    }

    MobileScreenLoader
    {
        id: speedmatchMobile

        anchors.fill: parent
        visible: false
        active: false
        asynchronous: true
        sourceComponent: speedmatchMobileComponent

        onVisibleChanged: if (visible) active = true
    }

    function startDiscovery()
    {
        if (!DecksController.categorised)
        {
            console.info("Discover: blocked,", DecksController.discoverBlockedReason)
            toast.show(DecksController.discoverBlockedReason)

            return
        }

        if (window.discovery.hasActiveSession)
        {
            console.info("Discover: continuing the unfinished round")
            window.pushScreen("discovery")

            return
        }

        window.pushScreen("discoverpick")
    }

    function startDiscoveryDecks(deckIds)
    {
        window.discovery.saveDecks(deckIds)

        if (!window.discovery.startDecks(deckIds))
        {
            toast.show(qsTr("Every word of these decks is already in My Vocabulary or known."))

            return
        }

        window.replaceScreen("discovery")
    }

    function startDeckDiscovery(deckId)
    {
        if (!window.discovery.startDeck(deckId))
        {
            toast.show(qsTr("Every word of this deck is already in My Vocabulary or known."))

            return
        }

        window.pushScreen("discovery")
    }

    Component
    {
        id: discoverPickMobileComponent

        MobileDiscoverPickScreen
        {
            anchors.fill: parent

            controller: window.discovery

            onStartRequested: function(deckIds) { window.startDiscoveryDecks(deckIds) }
            onDone: window.popScreen()
        }
    }

    Loader
    {
        id: discoverPickMobile

        anchors.fill: parent
        visible: false
        active: false
        asynchronous: false
        sourceComponent: discoverPickMobileComponent

        onVisibleChanged:
        {
            if (!visible)
            {
                return
            }

            active = true
            discoverPickMobile.item.restoreSaved()
        }
    }

    Component
    {
        id: discoveryMobileComponent

        MobileDiscoveryScreen
        {
            anchors.fill: parent

            controller: window.discovery
            store: window.store

            onWordTapped: function(word, sentence) { window.openLookup(word, sentence) }

            onEditRequested: function(card)
            {
                dictionaryAddWordDialogMobile.openForCard(window.discovery, card.id, card.targetWord, card.translation,
                                                          card.gender, card.partOfSpeech, card.box)
                window.pushScreen("dictionaryaddword")
            }

            onDone:
            {
                todayMobile.refresh()
                decksMobile.refresh()
                window.popScreen()
            }
        }
    }

    Loader
    {
        id: discoveryMobile

        anchors.fill: parent
        visible: false
        active: false
        asynchronous: false
        sourceComponent: discoveryMobileComponent

        onVisibleChanged: if (visible) active = true
    }

    Component
    {
        id: sentenceTrainingMobileComponent

        MobileSentenceTrainingScreen
        {
            anchors.fill: parent

            controller: window.sentenceTraining

            onDone: window.popScreen()

            onExerciseRequested: function(exercise)
            {
                if (window.sentenceTraining.start(exercise))
                {
                    window.pushScreen("sentencesession")
                }
            }

            onTabSelected: function(key)
            {
                window.resetTo(key)
            }
        }
    }

    Loader
    {
        id: sentenceTrainingMobile

        anchors.fill: parent
        visible: false
        active: false
        asynchronous: false
        sourceComponent: sentenceTrainingMobileComponent

        onVisibleChanged: if (visible) active = true
    }

    MobileSentenceSessionScreen
    {
        id: sentenceSessionMobile

        explainLookup: window.controllers.dictionary("sentenceExplain")

        anchors.fill: parent
        visible: false

        store: window.store
        controller: window.sentenceTraining

        onDone: window.popScreen()
        onLookupRequested: function(word, sentence) { window.openLookup(word, sentence) }
    }

    MobileIntensiveLearningScreen
    {
        id: intensiveMobile

        controller: window.training
        session: window.daily

        anchors.fill: parent
        visible: false

        store: window.store

        onDone:
        {
            AppReminder.syncDailyProgress()
            vocabularyMobile.refresh()

            window.popScreen()
        }

        onEditCardRequested: function(exercise) { window.editSessionCard(exercise) }

        onLookupRequested: function(word, sentence) { window.openLookup(word, sentence) }
    }

    MobileScreenMotion { target: todayMobile; screenName: "today"; router: window }
    MobileScreenMotion { target: wordListMobile; screenName: "wordlist"; router: window }
    MobileScreenMotion { target: settingsMobile; screenName: "settings"; router: window }
    MobileScreenMotion { target: dictionaryMobile; screenName: "dictionary"; router: window }
    MobileScreenMotion { target: readerMobile; screenName: "reader"; router: window }
    MobileScreenMotion { target: practiceMobile; screenName: "practice"; router: window }
    MobileScreenMotion { target: dictionaryWordMobile; screenName: "dictionaryword"; router: window }
    MobileScreenMotion { target: vocabularyMobile; screenName: "vocabulary"; router: window }
    MobileScreenMotion { target: decksMobile; screenName: "decks"; router: window }
    MobileScreenMotion { target: deckMobile; screenName: "deck"; router: window }
    MobileScreenMotion { target: triageMobile; screenName: "triage"; router: window }
    MobileScreenMotion { target: discoverPickMobile; screenName: "discoverpick"; router: window }
    MobileScreenMotion { target: discoveryMobile; screenName: "discovery"; router: window }
    MobileScreenMotion { target: repetitionMobile; screenName: "repetition"; router: window }
    MobileScreenMotion { target: addWordDialogMobile; screenName: "addword"; router: window }
    MobileScreenMotion { target: dictionaryAddWordDialogMobile; screenName: "dictionaryaddword"; router: window }
    MobileScreenMotion { target: flashcardsMobile; screenName: "flashcards"; router: window }
    MobileScreenMotion { target: flipMobile; screenName: "flip"; router: window }
    MobileScreenMotion { target: correctIncorrectMobile; screenName: "correctincorrect"; router: window }
    MobileScreenMotion { target: choiceMobile; screenName: "choice"; router: window }
    MobileScreenMotion { target: writtenMobile; screenName: "written"; router: window }
    MobileScreenMotion { target: audioMobile; screenName: "audio"; router: window }
    MobileScreenMotion { target: speedmatchMobile; screenName: "speedmatch"; router: window }
    MobileScreenMotion { target: sentenceTrainingMobile; screenName: "sentences"; router: window }
    MobileScreenMotion { target: sentenceSessionMobile; screenName: "sentencesession"; router: window }
    MobileScreenMotion { target: intensiveMobile; screenName: "intensive"; router: window }
    MobileScreenMotion { target: placementMobile; screenName: "placement"; router: window }

    UiProbe
    {
        id: snapshotProbe
    }

    Component
    {
        id: placementMobileComponent

        MobileOnboardingPlacementScreen
        {
            anchors.fill: parent

            controller: window.placementTest
            targetLanguage: window.store.targetLanguageCode
            retake: window.placementRetake

            onFinished:
            {
                if (window.placementRetake)
                {
                    window.popScreen()

                    return
                }

                window.resetTo("today")
            }
        }
    }

    Loader
    {
        id: placementMobile

        anchors.fill: parent
        visible: false
        active: false
        asynchronous: false
        sourceComponent: placementMobileComponent

        onVisibleChanged: if (visible) active = true
    }

    readonly property bool onboardingActive: window.screen === "onboarding"
    readonly property string onboardingStep: AppSyncController.restoring ? "restore" : AppOnboarding.step

    Component
    {
        id: onboardingSpeakComponent

        MobileOnboardingSpeakScreen {}
    }

    Loader
    {
        anchors.fill: parent
        z: 60
        active: false
        visible: window.onboardingActive && window.onboardingStep === "speak"
        sourceComponent: onboardingSpeakComponent

        onVisibleChanged: if (visible) active = true
        Component.onCompleted: if (visible) active = true
    }

    Component
    {
        id: onboardingSignInComponent

        MobileOnboardingSignInScreen
        {
            onContinued: AppOnboarding.next()
        }
    }

    Loader
    {
        anchors.fill: parent
        z: 60
        active: false
        visible: window.onboardingActive && window.onboardingStep === "signin"
        sourceComponent: onboardingSignInComponent

        onVisibleChanged: if (visible) active = true
        Component.onCompleted: if (visible) active = true
    }

    Component
    {
        id: onboardingLearnComponent

        MobileOnboardingLearnScreen {}
    }

    Loader
    {
        anchors.fill: parent
        z: 60
        active: false
        visible: window.onboardingActive && window.onboardingStep === "learn"
        sourceComponent: onboardingLearnComponent

        onVisibleChanged: if (visible) active = true
        Component.onCompleted: if (visible) active = true
    }

    Component
    {
        id: onboardingDailyComponent

        MobileOnboardingDailyScreen
        {
            store: window.store
        }
    }

    Loader
    {
        anchors.fill: parent
        z: 60
        active: false
        visible: window.onboardingActive && window.onboardingStep === "daily"
        sourceComponent: onboardingDailyComponent

        onVisibleChanged: if (visible) active = true
        Component.onCompleted: if (visible) active = true
    }

    Component
    {
        id: onboardingPlacementComponent

        MobileOnboardingPlacementScreen
        {
            controller: window.placementTest
            targetLanguage: AppOnboarding.chosenLanguage
            nativeLanguage: AppOnboarding.chosenNative
            showsBack: AppOnboarding.canGoBack

            onFinished: AppOnboarding.next()
            onBackRequested: AppOnboarding.back()
        }
    }

    Loader
    {
        id: onboardingPlacement

        anchors.fill: parent
        z: 60
        active: false
        visible: window.onboardingActive && window.onboardingStep === "placement"
        sourceComponent: onboardingPlacementComponent

        onVisibleChanged: if (visible) active = true
        Component.onCompleted: if (visible) active = true
    }

    Loader
    {
        anchors.fill: parent
        z: 60
        active: visible
        visible: window.onboardingActive && window.onboardingStep === "restore"

        sourceComponent: MobileOnboardingRestoreScreen {}
    }

    Component
    {
        id: onboardingDownloadComponent

        MobileOnboardingDownloadScreen
        {
            store: window.store

            onDone:
            {
                AppOnboarding.finish()
                window.resetTo("today")
            }
        }
    }

    Loader
    {
        anchors.fill: parent
        z: 60
        active: false
        visible: window.onboardingActive && window.onboardingStep === "download"
        sourceComponent: onboardingDownloadComponent

        onVisibleChanged: if (visible) active = true
        Component.onCompleted: if (visible) active = true
    }

    MobileSheetLoader
    {
        anchors.fill: parent
        z: 70
        open: window.tenseSheetTraining.length > 0

        sourceComponent: MobileTenseSheet
        {
            tenses: window.tenseSheetTenses

            onDismissed: window.tenseSheetTraining = ""

            onChosen: function(tense)
            {
                const trainingId = window.tenseSheetTraining

                window.tenseSheetTraining = ""
                window.startTenseTraining(trainingId, tense)
            }
        }
    }

    Component
    {
        id: dictionaryUpdateSheetComponent

        MobileDictionaryUpdateSheet
        {
            anchors.fill: parent
        }
    }

    Loader
    {
        id: dictionaryUpdateSheet

        anchors.fill: parent
        z: 70
        active: false
        asynchronous: false
        sourceComponent: dictionaryUpdateSheetComponent

        function offer()
        {
            dictionaryUpdateSheet.active = true
            dictionaryUpdateSheet.item.offer()
        }
    }

    Connections
    {
        target: AppCatalog

        function onCatalogChanged()
        {
            if (window.screen === "onboarding" || window.offeredDictionaryUpdate)
            {
                return
            }

            if (Object.keys(AppCatalog.dictionaryUpdate).length === 0)
            {
                return
            }

            window.offeredDictionaryUpdate = true

            dictionaryUpdateSheet.offer()
        }
    }

    WelcomeScreen
    {
        id: welcome

        objectName: "welcome"

        anchors.fill: parent
        z: 90
        shown: false

        name: AppSyncController.accountName.split(" ")[0] || ""
        reviewCount: todayMobile.dueCount
        newCount: todayMobile.wordsLeftForGoal

        onFinished: console.info("Welcome screen finished")
    }

    MobileLookupSheet
    {
        id: lookupMobile

        lookup: window.controllers.dictionary("lookup")
        stageCount: window.store.stageCount

        anchors.fill: parent

        onAddWordRequested: function(word, translation, partOfSpeech, hint)
        {
            dictionaryAddWordDialogMobile.openWith(word, translation, partOfSpeech, "", hint)
            window.showScreen("dictionaryaddword")
        }

        onEntryOpened: function(headword, partOfSpeech, hint)
        {
            dictionaryWordMobile.open(headword, partOfSpeech, "", hint)
            window.showScreen("dictionaryword")
        }
    }

    MobileToast
    {
        id: toast

        anchors.bottomMargin: AppMetrics.buttonHeightLarge + (AppMetrics.contentPadding * 3)
        duration: 6000
        z: 100
    }

    Connections
    {
        target: AppPackageDownloader

        function onPackageInstalled(id)
        {
            if (window.screen === "onboarding")
            {
                return
            }

            toast.show(qsTr("%1 is ready.").arg(AppCatalog.describePackage(id)))
        }

        function onPackageFailed(id, error)
        {
            if (window.screen === "onboarding")
            {
                return
            }

            toast.show(qsTr("%1 could not be installed.").arg(AppCatalog.describePackage(id)))
        }
    }

}
