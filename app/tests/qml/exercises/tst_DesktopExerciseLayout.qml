import QtQuick
import QtQuick.Layouts
import QtTest

TestCase
{
    id: test
    name: "DesktopExerciseLayout"
    width: 1280
    height: 744
    visible: true
    when: windowShown

    Component
    {
        id: fixture

        DesktopExerciseCard
        {
            width: test.width
            height: test.height
            property real promptContentHeight: 48
            property real answerContentHeight: 52

            primary: Rectangle
            {
                objectName: "prompt"
                Layout.fillWidth: true
                implicitHeight: promptContentHeight
            }

            secondary: Rectangle
            {
                objectName: "answer"
                Layout.fillWidth: true
                implicitHeight: answerContentHeight
            }
        }
    }

    function test_contentDoesNotMoveSplit_data()
    {
        return [{tag: "normal", height: 744}, {tag: "short", height: 584}, {tag: "tall", height: 1024}]
    }

    function test_contentDoesNotMoveSplit(data)
    {
        failOnWarning(/.*/)
        const card = createTemporaryObject(fixture, test, {height: data.height})
        verify(card !== null)
        const prompt = findChild(card, "prompt")
        const answer = findChild(card, "answer")
        wait(30)
        const originalTop = card.surface.y
        const originalHeight = card.surface.height
        const answerTop = answer.mapToItem(card, 0, 0).y
        const promptBottom = prompt.mapToItem(card, 0, prompt.height).y
        for (const height of [96, 240, 420, 48])
        {
            card.promptContentHeight = height
            card.answerContentHeight = height
            wait(30)
            compare(card.surface.y, originalTop)
            compare(card.surface.height, originalHeight)
            compare(answer.mapToItem(card, 0, 0).y, answerTop)
            verify(Math.abs(prompt.mapToItem(card, 0, prompt.height).y - promptBottom) <= 1)
        }
    }

    function test_primaryAndSecondaryHaveEqualSpace()
    {
        const card = createTemporaryObject(fixture, test)
        wait(30)
        const prompt = findChild(card, "prompt")
        const answer = findChild(card, "answer")
        compare(prompt.parent.parent.height, answer.parent.parent.height)
    }

    Component
    {
        id: fitFixture

        DesktopSentenceFit { width: 350; availableHeight: 120 }
    }

    function test_longSentencesShrinkWithinReadableLimits()
    {
        failOnWarning(/.*/)
        const fit = createTemporaryObject(fitFixture, test)
        fit.foreignText = "Bonjour."
        wait(30)
        compare(fit.foreignSize, 24)
        fit.foreignText = "La jeune femme voulait comprendre pourquoi les enfants avaient quitté la maison si tôt ce matin."
        fit.nativeText = "The young woman wanted to understand why the children had left the house so early this morning."
        wait(30)
        verify(fit.foreignSize < 24)
        verify(fit.foreignSize >= 18)
        verify(fit.nativeSize >= 16)
        fit.availableHeight = 600
        wait(30)
        compare(fit.foreignSize, 24)
    }

    Component
    {
        id: chipFixture

        DesktopChipArea
        {
            width: 220
            height: implicitHeight
            property int chipCount: 1
            property real chipWidth: 100
            spacing: 8

            Repeater
            {
                model: chipCount
                Rectangle { width: chipWidth; height: 36 }
            }
        }
    }

    function test_chipsGrowToThreeRowsThenScroll()
    {
        failOnWarning(/.*/)
        const chips = createTemporaryObject(chipFixture, test)
        wait(30)
        compare(chips.height, 36)
        chips.chipCount = 5
        wait(30)
        compare(chips.height, 124)
        chips.chipCount = 10
        wait(30)
        compare(chips.height, 124)
        verify(chips.contentHeight > chips.height)
        chips.showLastRow()
        verify(chips.contentY > 0)
    }

    function test_chipsRewrapWhenTheirWidthChanges()
    {
        failOnWarning(/.*/)
        const chips = createTemporaryObject(chipFixture, test, {chipCount: 2})
        wait(30)
        compare(chips.height, 36)
        chips.chipWidth = 150
        wait(30)
        compare(chips.height, 80)
        chips.chipWidth = 100
        wait(30)
        compare(chips.height, 36)
    }
}
