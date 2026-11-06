#include <catch2/catch_test_macros.hpp>

#include "FTCardTimer.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QObject>

/*
 * The clock three exercises score against. The rule it carries is that the countdown runs only
 * while an answer is actually awaited, which is what stops the score on screen falling during
 * feedback or after a run has ended.
 *
 * Timing assertions here are margins rather than exact counts: the tick is driven by a real event
 * loop, so how many fire in a given wait is not something to pin down.
 */

namespace
{
    /** Runs the event loop for roughly this long, so queued timer ticks actually arrive. */
    void PumpFor(int Milliseconds)
    {
        QElapsedTimer Waited;
        Waited.start();

        while (Waited.elapsed() < Milliseconds)
        {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        }
    }
}

TEST_CASE("A card's elapsed time starts near zero and grows", "[cardtimer]")
{
    QObject Owner;
    FTCardTimer Timer;
    Timer.Install(&Owner, []() {});

    Timer.BeginCard(true);
    CHECK(Timer.GetElapsed().count() < 50);

    PumpFor(120);

    // Only that time passed, not how much: this is a wall clock, not a fixed step.
    CHECK(Timer.GetElapsed().count() >= 100);
}

TEST_CASE("The countdown ticks while an answer is awaited", "[cardtimer]")
{
    QObject Owner;
    FTCardTimer Timer;

    int Ticks = 0;
    Timer.Install(&Owner, [&Ticks]() { ++Ticks; });

    Timer.BeginCard(true);
    PumpFor(250);

    CHECK(Ticks > 0);
}

TEST_CASE("No countdown runs when no answer is awaited", "[cardtimer]")
{
    // Feedback and a finished run both land here. A tick during either would keep dropping the
    // score on screen while nothing is being asked.
    QObject Owner;
    FTCardTimer Timer;

    int Ticks = 0;
    Timer.Install(&Owner, [&Ticks]() { ++Ticks; });

    Timer.BeginCard(false);
    PumpFor(250);

    CHECK(Ticks == 0);
}

TEST_CASE("Stopping ends the countdown", "[cardtimer]")
{
    QObject Owner;
    FTCardTimer Timer;

    int Ticks = 0;
    Timer.Install(&Owner, [&Ticks]() { ++Ticks; });

    Timer.BeginCard(true);
    PumpFor(150);

    Timer.Stop();
    const int AfterStop = Ticks;

    PumpFor(250);

    CHECK(Ticks == AfterStop);
}

TEST_CASE("A new card restarts the clock", "[cardtimer]")
{
    QObject Owner;
    FTCardTimer Timer;
    Timer.Install(&Owner, []() {});

    Timer.BeginCard(true);
    PumpFor(150);
    REQUIRE(Timer.GetElapsed().count() >= 100);

    // The next card is scored from its own start, not from however long the previous one took.
    Timer.BeginCard(true);

    CHECK(Timer.GetElapsed().count() < 50);
}

TEST_CASE("A stopped countdown starts again on the next card", "[cardtimer]")
{
    QObject Owner;
    FTCardTimer Timer;

    int Ticks = 0;
    Timer.Install(&Owner, [&Ticks]() { ++Ticks; });

    Timer.BeginCard(true);
    Timer.Stop();

    PumpFor(120);
    REQUIRE(Ticks == 0);

    Timer.BeginCard(true);
    PumpFor(250);

    CHECK(Ticks > 0);
}
