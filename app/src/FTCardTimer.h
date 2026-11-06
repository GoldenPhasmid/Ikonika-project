#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

#include <chrono>
#include <utility>

class FTCardTimer
{
public:
    template <typename OnTick>
    void Install(QObject* Owner, OnTick&& Tick)
    {
        CountdownTimer.setInterval(IntervalMilliseconds);

        QObject::connect(&CountdownTimer, &QTimer::timeout, Owner, std::forward<OnTick>(Tick));
    }

    /** A new card is up. */
    void BeginCard(bool bAwaitingAnswer)
    {
        PromptTimer.restart();

        if (bAwaitingAnswer)
        {
            CountdownTimer.start();

            return;
        }

        CountdownTimer.stop();
    }

    /** An answer landed, so the countdown has nothing left to count down. */
    void Stop()
    {
        CountdownTimer.stop();
    }

    std::chrono::milliseconds GetElapsed() const
    {
        return std::chrono::milliseconds(PromptTimer.elapsed());
    }

private:
    /** Fine enough that the countdown reads as continuous, coarse enough not to redraw for nothing. */
    static constexpr int IntervalMilliseconds = 100;

    QElapsedTimer PromptTimer;
    QTimer CountdownTimer;
};
