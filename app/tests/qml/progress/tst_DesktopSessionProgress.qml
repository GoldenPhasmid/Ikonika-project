import QtQuick
import QtTest

TestCase
{
    id: test
    name: "DesktopSessionProgress"
    width: 800
    height: 100
    visible: true
    when: windowShown

    Component
    {
        id: fixture
        DesktopSessionProgress { width: 800; stageCount: 5; stage: 1 }
    }

    function test_dashesKeepEqualWidthsAcrossStages()
    {
        failOnWarning(/.*/)
        const bar = createTemporaryObject(fixture, test)
        verify(bar !== null)
        const row = findChild(bar, "progress_segments")
        wait(0)
        const dashes = row.children.filter(item => item.index !== undefined)
        compare(dashes.length, 5)
        const originalWidth = dashes[0].width

        for (let stage = 1; stage <= 5; ++stage)
        {
            bar.stage = stage
            wait(0)
            for (const dash of dashes)
            {
                compare(dash.width, originalWidth)
                verify(dash.width > 0)
                if (dash.index + 1 === stage)
                {
                    compare(dash.color, AppTheme.colourText)
                }
                else
                {
                    verify(dash.color !== AppTheme.colourText)
                }
            }
        }
    }

    function test_currentDashFillsWithoutChangingItsWidth()
    {
        failOnWarning(/.*/)
        const bar = createTemporaryObject(fixture, test)
        verify(bar !== null)
        bar.stage = 2
        const row = findChild(bar, "progress_segments")
        wait(0)
        const dashes = row.children.filter(item => item.index !== undefined)
        const dash = dashes[1]
        const width = dash.width
        const fill = findChild(dash, "stage_fill")
        verify(fill !== null)

        for (const progress of [0, 0.25, 0.75, 1])
        {
            bar.stageProgress = progress
            tryCompare(fill, "width", width * progress)
            compare(dash.width, width)
            compare(dash.color, AppTheme.colourText)
            compare(fill.color, AppTheme.colourAccent)
        }
    }
}
