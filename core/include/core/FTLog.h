#pragma once

#include <QElapsedTimer>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(LogDatabase)
Q_DECLARE_LOGGING_CATEGORY(LogSchedule)
Q_DECLARE_LOGGING_CATEGORY(LogSpeech)
Q_DECLARE_LOGGING_CATEGORY(LogTranslate)
Q_DECLARE_LOGGING_CATEGORY(LogSync)
Q_DECLARE_LOGGING_CATEGORY(LogApp)
Q_DECLARE_LOGGING_CATEGORY(LogUpdate)

/** Session and exercise lifecycle: started, completed, stage transitions, and every answer. */
Q_DECLARE_LOGGING_CATEGORY(LogSession)

/** The YouTube reader: video/caption fetches and playback sync. */
Q_DECLARE_LOGGING_CATEGORY(LogYoutube)

/**
 * Logs how long the database call it sits in took, on destruction. One of these at the top of a
 * function is the whole measurement: the cost of a query is almost always its first Step(), where
 * SQLite does the actual scan, so timing the function rather than any one statement inside it
 * catches that without having to know which statement it is.
 *
 * Logs through LogDatabase at qCDebug, one level below the qCInfo/qCWarning/qCCritical calls
 * LogDatabase's other messages use, so a build that only wants failures and real events can drop
 * to qCInfo without losing them, while a slow open still shows by default alongside them.
 */
class FTQueryTimer
{
public:
    explicit FTQueryTimer(const char* InLabel);
    ~FTQueryTimer();

    FTQueryTimer(const FTQueryTimer&) = delete;
    FTQueryTimer& operator=(const FTQueryTimer&) = delete;

private:
    const char* Label;
    QElapsedTimer Timer;
};
