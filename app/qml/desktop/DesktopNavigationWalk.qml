import QtQuick

Item
{
    id: root

    required property var window

    readonly property bool requested: probe.environmentVariable("IKONIKA_NAV_WALK") === "1"

    property var steps: []
    property int currentStep: 0

    signal finished()

    UiProbe
    {
        id: probe
    }

    function press(objectName)
    {
        const item = probe.findChild(root.window.contentItem, objectName)

        if (!item)
        {
            probe.check(false, "control found: " + objectName)

            return
        }

        if (!root.window.visible || item.width === 0)
        {
            item.clicked()

            return
        }

        probe.click(item)
    }

    function pushRoundTrip(list, destination, screen)
    {
        list.push({
            label: "today -> " + destination,
            run: function() { root.press("nav_" + destination) },
            check: function() { probe.check(root.window.screen === screen, "today -> " + destination) }
        })

        list.push({
            label: destination + " -> today",
            run: function() { root.press("nav_today") },
            check: function() { probe.check(root.window.screen === "today", destination + " -> today") }
        })
    }

    function pushSession(list, label, start, origin)
    {
        list.push({
            label: label,
            run: start,
            check: function()
            {
                const screen = root.window.screen

                if (screen !== "training" && screen !== "review" && screen !== "triage")
                {
                    probe.stamp("nav: " + label + " did not start (screen " + screen + "), likely no cards")

                    return
                }

                probe.check(root.window.focusMode, label + " enters focus mode on " + screen)

                if (screen === "triage")
                {
                    root.window.testHooks.triage.leaveRequested()
                }
                else
                {
                    root.window.testHooks.session.leaveRequested()
                }

                probe.check(root.window.screen === origin, label + " leaves back to " + origin)
            }
        })
    }

    function pushDialog(list, label, open, dialog)
    {
        list.push({
            label: label,
            run: open,
            check: function()
            {
                probe.check(dialog.visible, label + " opens")

                dialog.close()
            }
        })
    }

    function buildSteps()
    {
        const list = []
        const hooks = root.window.testHooks

        root.pushRoundTrip(list, "vocabulary", "vocabulary")
        root.pushRoundTrip(list, "words", "words")
        root.pushRoundTrip(list, "reader", "reader")
        root.pushRoundTrip(list, "settings", "settings")

        list.push({
            label: "look up a word",
            run: function() { root.window.lookUpWord("bonjour") },
            check: function() { probe.check(root.window.screen === "words", "lookUpWord opens words") }
        })

        root.pushDialog(list, "add word from words", function() { root.window.createItem() }, hooks.wordDialog)

        list.push({
            label: "words -> today",
            run: function() { root.press("nav_today") },
            check: function() { probe.check(root.window.screen === "today", "words -> today") }
        })

        root.pushSession(list, "training from today", function() { hooks.today.learnRequested() }, "today")
        root.pushSession(list, "review from today", function() { hooks.today.reviewRequested() }, "today")

        list.push({
            label: "open my vocabulary",
            run: function() { root.press("nav_vocabulary") },
            check: function() { probe.check(root.window.screen === "vocabulary", "vocabulary opens my vocabulary") }
        })

        root.pushDialog(list, "add word to vocabulary", function() { root.window.createItem() }, hooks.wordDialog)

        list.push({
            label: "vocabulary -> today",
            run: function() { root.press("nav_today") },
            check: function() { probe.check(root.window.screen === "today", "vocabulary -> today") }
        })

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

        root.runNext()
    }

    Timer
    {
        id: settleTimer

        interval: 500

        onTriggered: root.evaluateAndAdvance()
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

        probe.stamp("nav: " + step.label)
        step.run()

        settleTimer.restart()
    }

    function evaluateAndAdvance()
    {
        root.steps[root.currentStep].check()

        ++root.currentStep

        root.runNext()
    }
}
