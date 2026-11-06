#include "core/FTLog.h"

Q_LOGGING_CATEGORY(LogDatabase, "ikonika.database")
Q_LOGGING_CATEGORY(LogSchedule, "ikonika.schedule")
Q_LOGGING_CATEGORY(LogSpeech, "ikonika.speech")
Q_LOGGING_CATEGORY(LogTranslate, "ikonika.translate")
Q_LOGGING_CATEGORY(LogSync, "ikonika.sync")
Q_LOGGING_CATEGORY(LogApp, "ikonika.app")
Q_LOGGING_CATEGORY(LogUpdate, "ikonika.update")
Q_LOGGING_CATEGORY(LogSession, "ikonika.session")
Q_LOGGING_CATEGORY(LogYoutube, "ikonika.youtube")

FTQueryTimer::FTQueryTimer(const char* InLabel)
    : Label(InLabel)
{
    Timer.start();
}

FTQueryTimer::~FTQueryTimer()
{
    qCDebug(LogDatabase) << Label << Timer.elapsed() << "ms";
}
