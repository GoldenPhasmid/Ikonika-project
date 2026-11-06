import QtQuick

/*
 * A scripted run over dictionary searches, so the cost of one is a number rather than an
 * impression. Each word is searched in turn, with the frames the scene graph produced while its
 * results were built recorded against it.
 *
 * The controller logs what it spent inside the search itself. This covers what it cannot see: the
 * delegates the screen builds for the entries that came back, which is where a word matching many
 * entries is felt.
 *
 * Set IKONIKA_DICTIONARY_BENCHMARK to a comma-separated word list to run it; it does nothing
 * otherwise.
 */
Item
{
    id: root

    /** The dictionary screen to exercise. */
    required property Item view

    readonly property var words: probe.environmentVariable("IKONIKA_DICTIONARY_BENCHMARK")
                                      .split(",")
                                      .map(function(word) { return word.trim() })
                                      .filter(function(word) { return word.length > 0 })

    readonly property bool requested: root.words.length > 0

    property int nextWord: 0

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

        root.nextWord = 0

        // The first search of a run pays for the dictionary's own page cache, which no later one
        // does. Started after the window has settled so that cost lands on a search rather than on
        // whatever the window was still doing.
        startTimer.restart()
    }

    function runNext()
    {
        if (root.nextWord >= root.words.length)
        {
            probe.stamp("dictionary benchmark done")

            return
        }

        const word = root.words[root.nextWord]
        ++root.nextWord

        probe.begin(word)
        probe.startFrames(root.view, word)
        probe.mark("search called")

        root.view.searchTerm(word)

        probe.mark("search returned")

        settleTimer.restart()
    }

    Timer
    {
        id: startTimer

        interval: 2000

        onTriggered: root.runNext()
    }

    Timer
    {
        id: settleTimer

        // Long enough for the frame carrying the results to be drawn and for the view to come to
        // rest, since a stall shows in the frames after the call as much as during it.
        interval: 2000

        onTriggered:
        {
            probe.mark("settled")
            probe.stopFrames()
            root.runNext()
        }
    }
}
