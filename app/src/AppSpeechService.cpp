#include "AppSpeechService.h"

#include "AppPaths.h"

#include "core/FTLog.h"
#include "core/FTSettings.h"

#include <QCoreApplication>
#include <QDir>
#include <QMetaObject>
#include <QMutexLocker>
#include <QSettings>
#include <QThread>
#include <QThreadPool>

#include <algorithm>

namespace
{
    std::vector<std::string> DiscoverModelDirectories()
    {
        const QStringList Roots = {
            AppPaths::DownloadedModelsDirectory(),
            QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("models"))
        };

        QStringList Claimed;
        std::vector<std::string> Directories;

        for (const QString& Root : Roots)
        {
            const QDir Models(Root);

            for (const QString& Name : Models.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
            {
                if (Claimed.contains(Name))
                {
                    continue;
                }

                if (!IsVoiceModelDirectory(Models.filePath(Name).toStdString()))
                {
                    continue;
                }

                Claimed.append(Name);
                Directories.push_back(Models.filePath(Name).toStdString());
            }
        }

        return Directories;
    }

    constexpr const char* VoiceNameKey = IKONIKA_AUDIO_GROUP "voiceName";
    constexpr const char* SpeakerKey = IKONIKA_AUDIO_GROUP "speaker";

    QString PreferredVoiceKey(const QString& LanguageCode)
    {
        return QStringLiteral(IKONIKA_AUDIO_GROUP "preferredVoice/") + LanguageCode;
    }

    constexpr unsigned long PlaybackPollMs = 50;
    constexpr unsigned long NarrationPollMs = 20;
}

AppSpeechService::AppSpeechService()
{
    const QSettings Settings(FTSettings::Get().OpenStore());

    RequestedSpeed = std::clamp(Settings.value(SpeedKey, 1.0).toFloat(), MinimumSpeechSpeed, MaximumSpeechSpeed);

    Service.SetSpeed(RequestedSpeed);
    Service.SetSentenceSpeed(AppSpeech::GetSentenceSpeed());
    Service.SetSpeakerId(Settings.value(SpeakerKey, 0).toInt());
    Service.SetEnabled(Settings.value(EnabledKey, true).toBool());
    Service.SetMaximumCacheEntries(FTSettings::Get().GetSpeechRules().CachedLineCount);
}

void AppSpeechService::ReloadFromSettings()
{
    const QSettings Settings(FTSettings::Get().OpenStore());
    const float Speed{Settings.value(SpeedKey, 1.0).toFloat()};
    const int Speaker{Settings.value(SpeakerKey, 0).toInt()};
    const bool bEnabled{Settings.value(EnabledKey, true).toBool()};

    qCInfo(LogSpeech) << "Speech settings reloaded for the open workspace, enabled =" << bEnabled;

    RequestedSpeed = std::clamp(Speed, MinimumSpeechSpeed, MaximumSpeechSpeed);

    const float SentenceSpeed{AppSpeech::GetSentenceSpeed()};

    ChangeOnWorker([this, Speed, SentenceSpeed, Speaker, bEnabled]()
    {
        Service.SetSpeed(Speed);
        Service.SetSentenceSpeed(SentenceSpeed);
        Service.SetEnabled(bEnabled);

        if (Service.IsAvailable())
        {
            Service.SetActiveVoice(FindSavedVoice());
            ApplyPreferredVoices();
        }

        Service.SetSpeakerId(Speaker);
    });
}

void AppSpeechService::EnsureLoaded()
{
    if (Service.IsAvailable() || !LoadVoices())
    {
        return;
    }

    Service.Prepare("a");

    QMetaObject::invokeMethod(this, &AppSpeech::ready, Qt::QueuedConnection);
}

bool AppSpeechService::LoadVoices()
{
    std::vector<FTVoiceModel> Models{Service.CreateVoiceModels(DiscoverModelDirectories())};

    const QSettings Settings(FTSettings::Get().OpenStore());
    const QMutexLocker Lock(&StateMutex);

    Service.AddVoiceModels(std::move(Models));

    if (!Service.IsAvailable())
    {
        return false;
    }

    Service.SetActiveVoice(FindSavedVoice());
    Service.SetSpeakerId(Settings.value(SpeakerKey, 0).toInt());
    ApplyPreferredVoices();

    return true;
}

void AppSpeechService::ApplyPreferredVoices()
{
    const QSettings Settings(FTSettings::Get().OpenStore());

    for (int Index = 0; Index < Service.GetVoiceCount(); ++Index)
    {
        const FTVoiceInfo Info{Service.GetVoiceInfo(Index)};

        if (Info.Engine == SupertonicEngine)
        {
            continue;
        }

        for (const std::string& Language : Info.Languages)
        {
            const QString LanguageCode{QString::fromStdString(Language)};
            const int Preferred{FindVoiceByName(Settings.value(PreferredVoiceKey(LanguageCode)).toString())};

            Service.SetPreferredVoice(Language, Preferred);

            if (Preferred >= 0)
            {
                qCInfo(LogSpeech) << "preferred voice for" << LanguageCode << "is"
                                  << QString::fromStdString(Service.GetVoiceInfo(Preferred).Name);
            }
        }
    }
}

void AppSpeechService::SetPreferredVoiceName(const QString& LanguageCode, const QString& Name)
{
    QSettings Stored(FTSettings::Get().OpenStore());

    if (Name.isEmpty())
    {
        Stored.remove(PreferredVoiceKey(LanguageCode));
        qCInfo(LogSpeech) << "preferred voice for" << LanguageCode << "cleared, the default voice speaks it";
    }
    else
    {
        Stored.setValue(PreferredVoiceKey(LanguageCode), Name);
        qCInfo(LogSpeech) << "preferred voice for" << LanguageCode << "set to" << Name;
    }

    ChangeOnWorker([this, LanguageCode, Name]()
    {
        Service.SetPreferredVoice(LanguageCode.toStdString(), FindVoiceByName(Name));
    });
}

QString AppSpeechService::GetPreferredVoiceName(const QString& LanguageCode) const
{
    return FTSettings::Get().OpenStore().value(PreferredVoiceKey(LanguageCode)).toString();
}

void AppSpeechService::ChangeOnWorker(std::function<void()> Apply)
{
    ++PendingGeneration;

    QThreadPool::globalInstance()->start([this, Apply = std::move(Apply)]()
    {
        const QMutexLocker WorkerLock(&WorkerMutex);
        const QMutexLocker StateLock(&StateMutex);

        Apply();
    });
}

void AppSpeechService::Prewarm()
{
    QThreadPool::globalInstance()->start([this]()
    {
        const QMutexLocker Lock(&WorkerMutex);

        EnsureLoaded();
    });
}

void AppSpeechService::Reload()
{
    QThreadPool::globalInstance()->start([this]()
    {
        const QMutexLocker Lock(&WorkerMutex);

        if (LoadVoices())
        {
            QMetaObject::invokeMethod(this, &AppSpeech::ready, Qt::QueuedConnection);
        }
    });
}

void AppSpeechService::Unload()
{
    ++PendingGeneration;

    QThreadPool::globalInstance()->start([this]()
    {
        const QMutexLocker WorkerLock(&WorkerMutex);

        if (!Service.IsAvailable())
        {
            return;
        }

        {
            const QMutexLocker StateLock(&StateMutex);

            Service.UnloadVoices();
        }

        QMetaObject::invokeMethod(this, &AppSpeech::ready, Qt::QueuedConnection);
    });
}

void AppSpeechService::SpeakNow(const FSpeechRequest& Request)
{
    if (bNarrationPaused)
    {
        qCInfo(LogSpeech) << "speech during paused read aloud drops the paused sentence";

        StopNarration();
    }

    const float RateScale = Request.RateScale;
    const std::string Utf8 = Request.Text.toStdString();
    const std::string Language = Request.LanguageCode.toStdString();

    const quint64 Generation = ++PendingGeneration;

    QThreadPool::globalInstance()->start([this, Utf8, Language, RateScale, Generation]()
    {
        if (Generation != PendingGeneration.loadRelaxed())
        {
            return;
        }

        {
            const QMutexLocker Lock(&WorkerMutex);

            if (Generation != PendingGeneration.loadRelaxed())
            {
                return;
            }

            EnsureLoaded();

            if (!Service.Speak(Utf8, Language, RateScale))
            {
                ReportSpeaking(false);

                return;
            }
        }

        ReportSpeaking(true);

        while (Generation == PendingGeneration.loadRelaxed() && IsServicePlaying())
        {
            QThread::msleep(PlaybackPollMs);
        }

        if (Generation == PendingGeneration.loadRelaxed())
        {
            ReportSpeaking(false);
        }
    });
}

void AppSpeechService::SetActiveVoice(int Index)
{
    ChangeOnWorker([this, Index]()
    {
        Service.SetActiveVoice(Index);

        QSettings Stored(FTSettings::Get().OpenStore());
        Stored.setValue(VoiceNameKey, QString::fromStdString(Service.GetVoiceInfo(Service.GetActiveVoice()).Name));
        Stored.setValue(SpeakerKey, Service.GetSpeakerId());
    });
}

int AppSpeechService::FindSavedVoice() const
{
    const int Saved{FindVoiceByName(FTSettings::Get().OpenStore().value(VoiceNameKey).toString())};

    if (Saved >= 0 && Service.GetVoiceInfo(Saved).Engine == SupertonicEngine)
    {
        return Saved;
    }

    return Service.FindFallbackVoice();
}

int AppSpeechService::FindVoiceByName(const QString& Name) const
{
    if (Name.isEmpty())
    {
        return -1;
    }

    for (int Index = 0; Index < Service.GetVoiceCount(); ++Index)
    {
        if (QString::fromStdString(Service.GetVoiceInfo(Index).Name) == Name)
        {
            return Index;
        }
    }

    return -1;
}

void AppSpeechService::SetSpeed(float Value)
{
    QSettings Settings(FTSettings::Get().OpenStore());
    Settings.setValue(SpeedKey, Value);

    RequestedSpeed = std::clamp(Value, MinimumSpeechSpeed, MaximumSpeechSpeed);

    ChangeOnWorker([this, Value]() { Service.SetSpeed(Value); });
}

void AppSpeechService::SetSentenceSpeed(float Value)
{
    AppSpeech::SetSentenceSpeed(Value);

    ChangeOnWorker([this, Value]() { Service.SetSentenceSpeed(Value); });
}

void AppSpeechService::SetSpeakerId(int Value)
{
    QSettings Settings(FTSettings::Get().OpenStore());
    Settings.setValue(SpeakerKey, Value);

    ChangeOnWorker([this, Value]() { Service.SetSpeakerId(Value); });
}

void AppSpeechService::SetEnabled(bool bValue)
{
    QSettings Stored(FTSettings::Get().OpenStore());
    Stored.setValue(EnabledKey, bValue);

    ChangeOnWorker([this, bValue]() { Service.SetEnabled(bValue); });

    emit enabledChanged(bValue);
}

void AppSpeechService::SetCachedLineCount(int Value)
{
    FTSettings::Get().SetCachedLineCount(Value);
    const int Sanitised = FTSettings::Get().GetSpeechRules().CachedLineCount;

    ChangeOnWorker([this, Sanitised]() { Service.SetMaximumCacheEntries(Sanitised); });
}

bool AppSpeechService::IsAvailable() const
{
    return Service.IsAvailable();
}

bool AppSpeechService::IsEnabled() const
{
    return Service.IsEnabled();
}

int AppSpeechService::GetVoiceCount() const
{
    return Service.GetVoiceCount();
}

FTVoiceInfo AppSpeechService::GetVoiceInfo(int Index) const
{
    return Service.GetVoiceInfo(Index);
}

int AppSpeechService::GetActiveVoice() const
{
    return Service.GetActiveVoice();
}

int AppSpeechService::GetSpeakerId() const
{
    return Service.GetSpeakerId();
}

float AppSpeechService::GetSpeed() const
{
    return RequestedSpeed;
}

QMutex& AppSpeechService::Lock()
{
    return StateMutex;
}

bool AppSpeechService::CanNarrate() const
{
    return Service.IsAvailable() && Service.IsEnabled();
}

void AppSpeechService::QueueNarration(int Id, const QString& Text, const QString& LanguageCode)
{
    const QMutexLocker Lock(&NarrationMutex);

    NarrationRequests.push_back({Id, Text.toStdString(), LanguageCode.toStdString()});

    if (bNarrationRunning)
    {
        return;
    }

    bNarrationRunning = true;

    QThreadPool::globalInstance()->start([this]() { RunNarration(); });
}

void AppSpeechService::StopNarration()
{
    ++NarrationGeneration;

    {
        const QMutexLocker Lock(&NarrationMutex);

        NarrationRequests.clear();
    }

    bNarrationPaused = false;

    const QMutexLocker StateLock(&StateMutex);

    Service.SetNarrationPaused(false);
    Service.Stop();
}

bool AppSpeechService::PauseNarration()
{
    {
        const QMutexLocker Lock(&NarrationMutex);

        if (!bNarrationRunning)
        {
            return false;
        }
    }

    bNarrationPaused = true;

    const QMutexLocker StateLock(&StateMutex);

    Service.SetNarrationPaused(true);

    return true;
}

bool AppSpeechService::ResumeNarration()
{
    if (!bNarrationPaused)
    {
        return false;
    }

    bNarrationPaused = false;

    const QMutexLocker StateLock(&StateMutex);

    Service.SetNarrationPaused(false);

    return true;
}

void AppSpeechService::RunNarration()
{
    FNarrationRun Run;
    Run.Generation = NarrationGeneration.loadRelaxed();

    for (;;)
    {
        if (Run.Generation != NarrationGeneration.loadRelaxed())
        {
            Run = FNarrationRun{};
            Run.Generation = NarrationGeneration.loadRelaxed();
        }

        ReportNarrationProgress(Run);

        const std::optional<FNarrationRequest> Request{TakeNarrationRequest(Run)};

        if (Request.has_value())
        {
            PlayNarrationRequest(*Request, Run);

            continue;
        }

        if (Run.Playing.empty())
        {
            const QMutexLocker Lock(&NarrationMutex);

            if (NarrationRequests.empty())
            {
                bNarrationRunning = false;

                return;
            }
        }

        QThread::msleep(NarrationPollMs);
    }
}

std::optional<AppSpeechService::FNarrationRequest> AppSpeechService::TakeNarrationRequest(FNarrationRun& Run)
{
    const QMutexLocker Lock(&NarrationMutex);

    if (NarrationRequests.empty() || Run.Generation != NarrationGeneration.loadRelaxed())
    {
        return std::nullopt;
    }

    FNarrationRequest Request{std::move(NarrationRequests.front())};
    NarrationRequests.pop_front();

    return Request;
}

void AppSpeechService::PlayNarrationRequest(const FNarrationRequest& Request, FNarrationRun& Run)
{
    std::vector<int16_t> Samples;

    {
        const QMutexLocker Lock(&WorkerMutex);

        EnsureLoaded();

        if (Run.VoiceIndex < 0)
        {
            Run.VoiceIndex = Service.FindVoiceForLanguage(Request.LanguageCode);
        }

        if (Run.VoiceIndex < 0 || !Service.IsEnabled())
        {
            qCWarning(LogSpeech) << "narration has no voice to speak with";

            ReportNarrationFailed();

            return;
        }

        Samples = Service.RenderNarration(Run.VoiceIndex, Request.Text, Request.LanguageCode);
    }

    if (Samples.empty())
    {
        qCWarning(LogSpeech) << "narration synthesis produced nothing, sentence skipped:"
                             << QString::fromStdString(Request.Text);

        ReportNarrationStarted(Request.Id);
        ReportNarrationFinished(Request.Id);

        return;
    }

    const QMutexLocker StateLock(&StateMutex);

    if (Run.Generation != NarrationGeneration.loadRelaxed())
    {
        return;
    }

    const std::optional<FTNarrationChunk> Chunk{Service.PlayNarration(Run.VoiceIndex, std::move(Samples), Run.bContinues)};

    if (!Chunk.has_value())
    {
        qCWarning(LogSpeech) << "narration could not reach the audio device";

        ReportNarrationFailed();

        return;
    }

    Run.bContinues = true;
    Run.Playing.push_back({Request.Id, *Chunk, false});
}

void AppSpeechService::ReportNarrationProgress(FNarrationRun& Run)
{
    if (Run.Playing.empty())
    {
        return;
    }

    uint64_t Position{0};
    bool bSounding{false};

    {
        const QMutexLocker StateLock(&StateMutex);

        if (Run.Generation != NarrationGeneration.loadRelaxed())
        {
            return;
        }

        Position = Service.GetNarrationPosition(Run.VoiceIndex);
        bSounding = Service.IsNarrationPlaying(Run.VoiceIndex);
    }

    for (FNarrationPlayback& Entry : Run.Playing)
    {
        if (!Entry.bStarted && (Position >= Entry.Chunk.StartFrame || !bSounding))
        {
            Entry.bStarted = true;

            ReportNarrationStarted(Entry.Id);
        }
    }

    while (!Run.Playing.empty() && (Position >= Run.Playing.front().Chunk.EndFrame || !bSounding))
    {
        ReportNarrationFinished(Run.Playing.front().Id);

        Run.Playing.pop_front();
    }
}

bool AppSpeechService::IsServicePlaying()
{
    const QMutexLocker Lock(&WorkerMutex);

    return Service.IsPlaying();
}
