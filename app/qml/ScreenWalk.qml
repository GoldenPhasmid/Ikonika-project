import QtQuick

Item
{
    id: root

    required property var window

    readonly property var screens: probe.environmentVariable("IKONIKA_SCREEN_WALK")
                                        .split(",")
                                        .map(function(name) { return name.trim() })
                                        .filter(function(name) { return name.length > 0 })

    readonly property bool requested: root.screens.length > 0

    property int currentScreen: 0
    property int totalWarnings: 0

    signal finished()

    UiProbe
    {
        id: probe
    }

    function start()
    {
        if (!root.requested)
        {
            return
        }

        root.currentScreen = 0
        root.totalWarnings = 0

        probe.resetWarnings()
        probe.stamp("screen walk: " + root.screens[0])

        settleTimer.restart()
    }

    Timer
    {
        id: settleTimer

        interval: 1000

        onTriggered: root.checkAndAdvance()
    }

    function checkAndAdvance()
    {
        const warnings = probe.warningCount()
        root.totalWarnings += warnings

        probe.stamp("screen " + root.screens[root.currentScreen] + " | warnings " + warnings)

        ++root.currentScreen

        if (root.currentScreen >= root.screens.length)
        {
            probe.stamp("screen walk done | total warnings " + root.totalWarnings)
            root.finished()

            return
        }

        probe.resetWarnings()
        root.window.resetTo(root.screens[root.currentScreen])

        settleTimer.restart()
    }
}
