import QtQuick

Item
{
    id: root

    required property var window

    readonly property bool requested: probe.environmentVariable("IKONIKA_NAV_WALK") === "1"

    property var steps: []
    property int currentStep: 0
    property int readyWaits: 0

    signal finished()

    UiProbe
    {
        id: probe
    }

    function navigate(destination)
    {
        const hook = root.window.testHooks[root.window.screen + "Mobile"]
        const scope = hook ? (hook.item || hook) : root.window.contentItem
        const item = probe.findChild(scope, "tab_" + destination)

        if (!item)
        {
            probe.check(false, "clickable control found for " + destination)

            return
        }

        if (item.width === 0 && hook && hook.item)
        {
            probe.stamp("nav: tab " + destination + " has no size in a minimised window, selecting it directly")
            hook.item.tabSelected(destination)

            return
        }

        probe.click(item)
    }

    function isScreenLoaded(destination)
    {
        const loader = root.window.testHooks[destination + "Mobile"]

        return !loader || loader.status === undefined || loader.status === Loader.Ready
    }

    function pushRoundTrip(list, destination, screen)
    {
        list.push({
            label: "today -> " + destination,
            run: function() { root.navigate(destination) },
            check: function()
            {
                probe.check(root.window.screen === screen, "today -> " + destination)
            }
        })

        list.push({
            label: destination + " -> today",
            ready: function() { return root.isScreenLoaded(screen) },
            run: function() { root.navigate("today") },
            check: function()
            {
                probe.check(root.window.screen === "today", destination + " -> today")
            }
        })
    }

    function checkOrigin(originScreen, label)
    {
        probe.check(root.window.screenBelow() === originScreen, label)
    }

    function finishTraining(originScreen)
    {
        const screen = root.window.screen
        const hooks = root.window.testHooks

        if (screen === "intensive")
        {
            root.checkOrigin(originScreen, "training origin is " + originScreen)

            hooks.intensiveMobile.done()

            probe.check(root.window.screen === originScreen, "intensive finish returns to " + originScreen)

            return
        }

        if (screen === "triage")
        {
            root.checkOrigin(originScreen, "triage origin is " + originScreen)

            hooks.triageMobile.item.done()

            probe.check(root.window.screen === originScreen, "triage abandon returns to " + originScreen)

            return
        }

        probe.stamp("nav: training from " + originScreen + " did not navigate (screen " + screen + ")")
    }

    function pushTraining(list, originScreen, hookName)
    {
        list.push({
            label: "train (" + originScreen + ")",
            run: function()
            {
                root.window.testHooks[hookName + "Mobile"].intensiveRequested()
            },
            check: function() { root.finishTraining(originScreen) }
        })
    }

    function pushExercise(list)
    {
        list.push({
            label: "exercise flashcards from practice (mobile)",
            run: function()
            {
                root.window.testHooks.vocabularyMobile.practiceRequested()
                root.window.testHooks.practiceMobile.item.exerciseRequested("flashcards", false, 10, 1)
            },
            check: function()
            {
                const screen = root.window.screen

                if (screen !== "flashcards")
                {
                    probe.stamp("nav: flashcards did not start (screen " + screen + "), likely no cards")

                    return
                }

                root.window.testHooks.flashcardsMobile.item.done()

                probe.check(root.window.screen === "practice",
                    "flashcards finish returns to practice, not vocabulary")

                root.window.testHooks.practiceMobile.item.done()

                probe.check(root.window.screen === "vocabulary",
                    "practice back returns to vocabulary")
            }
        })
    }

    function pushRepetition(list)
    {
        list.push({
            label: "repetition from today",
            run: function()
            {
                root.window.resetTo("today")
                root.window.testHooks.todayMobile.repetitionRequested()
            },
            check: function()
            {
                const screen = root.window.screen

                if (screen !== "repetition")
                {
                    probe.stamp("nav: repetition did not start (screen " + screen + "), nothing due")

                    return
                }

                root.window.testHooks.repetitionMobile.done()

                probe.check(root.window.screen === "today", "repetition finish returns to today")
            }
        })
    }

    function pushMobileOnlyScreens(list)
    {
        list.push({
            label: "vocabulary before mobile-only routes",
            run: function() { root.window.openVocabulary("") },
            check: function()
            {
                probe.check(root.window.screen === "vocabulary", "vocabulary reopens after repetition")
            }
        })

        list.push({
            label: "add word from vocabulary (mobile)",
            run: function() { root.window.testHooks.vocabularyMobile.addWordRequested() },
            check: function()
            {
                probe.check(root.window.screen === "dictionary", "vocabulary add word opens dictionary")
            }
        })

        list.push({
            label: "stack depth: dictionary -> word -> add word unwinds one level at a time (mobile)",
            run: function() { root.window.testHooks.vocabularyMobile.searchRequested() },
            check: function()
            {
                const hooks = root.window.testHooks

                probe.check(root.window.screen === "dictionary", "search from vocabulary opens dictionary")

                hooks.dictionaryMobile.item.entryOpened("bonjour", "", "")
                probe.check(root.window.screen === "dictionaryword", "dictionary opens word screen")
                probe.check(root.window.screenBelow() === "dictionary", "word screen stacked on dictionary")

                hooks.dictionaryWordMobile.item.addWordRequested("bonjour", "hello", "", "")
                probe.check(root.window.screen === "dictionaryaddword", "word screen opens add word")
                probe.check(root.window.screenBelow() === "dictionaryword",
                    "add word stacked on word screen")

                hooks.dictionaryAddWordDialogMobile.item.done()
                probe.check(root.window.screen === "dictionaryword",
                    "back from add word returns to word screen")

                hooks.dictionaryWordMobile.item.done()
                probe.check(root.window.screen === "dictionary",
                    "back from word screen returns to dictionary")

                hooks.dictionaryMobile.item.done()
                probe.check(root.window.screen === "today", "back from dictionary returns to today")
            }
        })
    }

    function buildSteps()
    {
        const list = []

        root.pushRoundTrip(list, "vocabulary", "vocabulary")
        root.pushRoundTrip(list, "dictionary", "dictionary")

        list.push({
            label: "today -> settings",
            run: function() { root.window.testHooks.todayMobile.settingsRequested() },
            check: function()
            {
                probe.check(root.window.screen === "settings", "today -> settings")
            }
        })

        list.push({
            label: "settings -> today",
            ready: function() { return root.isScreenLoaded("settings") },
            run: function() { root.window.testHooks.settingsMobile.item.done() },
            check: function()
            {
                probe.check(root.window.screen === "today", "settings -> today")
            }
        })

        list.push({
            label: "search word opens dictionary",
            run: function() { root.window.lookUpWord("bonjour", "") },
            check: function()
            {
                probe.check(root.window.screen === "dictionary", "lookUpWord opens dictionary")
            }
        })

        list.push({
            label: "back to today before vocabulary routes",
            run: function()
            {
                root.window.testHooks.dictionaryMobile.item.done()
            },
            check: function()
            {
                probe.check(root.window.screen === "today", "dictionary back returns to today")
            }
        })

        root.pushTraining(list, "today", "today")

        list.push({
            label: "open vocabulary",
            run: function() { root.window.openVocabulary("") },
            check: function()
            {
                probe.check(root.window.screen === "vocabulary", "openVocabulary switches screen")
            }
        })

        root.pushExercise(list)
        root.pushRepetition(list)
        root.pushMobileOnlyScreens(list)

        return list
    }

    function start()
    {
        if (!root.requested)
        {
            return
        }

        root.steps = root.buildSteps()
        root.currentStep = 0

        runNext()
    }

    Timer
    {
        id: settleTimer

        interval: 500

        onTriggered: root.evaluateAndAdvance()
    }

    Timer
    {
        id: readyTimer

        interval: 100

        onTriggered: root.runNext()
    }

    function runNext()
    {
        if (root.currentStep >= root.steps.length)
        {
            probe.stamp("navigation walk done")
            root.finished()

            return
        }

        const step = root.steps[root.currentStep]

        if (step.ready && !step.ready() && root.readyWaits < 30)
        {
            ++root.readyWaits
            readyTimer.restart()

            return
        }

        root.readyWaits = 0

        probe.stamp("nav: " + step.label)
        step.run()

        settleTimer.restart()
    }

    function evaluateAndAdvance()
    {
        root.steps[root.currentStep].check()

        ++root.currentStep

        runNext()
    }
}
