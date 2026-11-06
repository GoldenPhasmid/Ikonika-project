#include "AppSpeech.h"

#include "core/FTLog.h"
#include "core/FTSettings.h"

#include <QMetaObject>

AppSpeech* AppSpeech::CurrentInstance = nullptr;

AppSpeech::AppSpeech()
{
    connect(this, &AppSpeech::speechReleased, this, &AppSpeech::SpeakPending);
}

AppSpeech& AppSpeech::Get()
{
    static AppSpeech NoOpInstance;

    return CurrentInstance != nullptr ? *CurrentInstance : NoOpInstance;
}

void AppSpeech::SetInstance(AppSpeech* NewInstance)
{
    CurrentInstance = NewInstance;
}

void AppSpeech::SpeakAtRate(const QString& Text, const QString& LanguageCode, float RateScale)
{
    if (Text.isEmpty() || LanguageCode.isEmpty())
    {
        return;
    }

    const bool bDispatchQueued = PendingRequest.has_value();

    PendingRequest = FSpeechRequest{Text, LanguageCode, RateScale};

    if (!bDispatchQueued)
    {
        QMetaObject::invokeMethod(this, &AppSpeech::SpeakPending, Qt::QueuedConnection);
    }
}

void AppSpeech::SpeakPending()
{
    if (!PendingRequest.has_value())
    {
        return;
    }

    if (IsSpeechHeld())
    {
        qCInfo(LogSpeech) << "speech held until the screen transition ends:" << PendingRequest->Text;

        return;
    }

    const FSpeechRequest Request = *PendingRequest;
    PendingRequest.reset();

    qCDebug(LogSpeech) << "speak request:" << Request.Text << "language:" << Request.LanguageCode;

    SpeakNow(Request);
}

void AppSpeech::ReportSpeaking(bool bValue)
{
    if (bSpeaking.fetchAndStoreRelaxed(bValue ? 1 : 0) == (bValue ? 1 : 0))
    {
        return;
    }

    QMetaObject::invokeMethod(this, [this, bValue]() { emit speakingChanged(bValue); },
                              Qt::QueuedConnection);
}

void AppSpeech::ReportNarrationStarted(int Id)
{
    QMetaObject::invokeMethod(this, [this, Id]() { emit narrationStarted(Id); }, Qt::QueuedConnection);
}

void AppSpeech::ReportNarrationFinished(int Id)
{
    QMetaObject::invokeMethod(this, [this, Id]() { emit narrationFinished(Id); }, Qt::QueuedConnection);
}

void AppSpeech::ReportNarrationFailed()
{
    QMetaObject::invokeMethod(this, [this]() { emit narrationFailed(); }, Qt::QueuedConnection);
}

void AppSpeech::ReportNarrationPlayRequested()
{
    QMetaObject::invokeMethod(this, [this]() { emit narrationPlayRequested(); }, Qt::QueuedConnection);
}

void AppSpeech::ReportNarrationPauseRequested()
{
    QMetaObject::invokeMethod(this, [this]() { emit narrationPauseRequested(); }, Qt::QueuedConnection);
}

void AppSpeech::ReportNarrationStopRequested()
{
    QMetaObject::invokeMethod(this, [this]() { emit narrationStopRequested(); }, Qt::QueuedConnection);
}

void AppSpeech::holdSpeech()
{
    ++HoldCount;
}

void AppSpeech::releaseSpeech()
{
    if (HoldCount == 0)
    {
        qCWarning(LogSpeech) << "Speech released with no hold on it";

        return;
    }

    if (--HoldCount == 0)
    {
        emit speechReleased();
    }
}

bool AppSpeech::IsSpeechHeld() const
{
    return HoldCount > 0;
}

void AppSpeech::SilenceSpeech()
{
    ++SilenceCount;
}

void AppSpeech::UnsilenceSpeech()
{
    if (SilenceCount == 0)
    {
        qCWarning(LogSpeech) << "Speech unsilenced with no silence on it";

        return;
    }

    --SilenceCount;
}

bool AppSpeech::IsSpeechSilenced() const
{
    return SilenceCount > 0;
}

bool AppSpeech::AllowsAutomaticSpeech() const
{
    if (FTSettings::Get().GetSpeechRules().bAutoSpeak)
    {
        return true;
    }

    qCInfo(LogSpeech) << "automatic speech skipped, turned off in settings";

    return false;
}
