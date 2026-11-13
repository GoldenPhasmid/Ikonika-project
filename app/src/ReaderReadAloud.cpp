#include "ReaderReadAloud.h"

#include "AppCatalog.h"
#include "AppSpeech.h"
#include "FTParagraphModel.h"

#include "core/FTLog.h"
#include "core/FTSettings.h"
#include "core/FTSpeech.h"

#include <QGuiApplication>

#include <algorithm>
#include <array>

static constexpr double NarrationLookaheadSeconds = 15.0;
static constexpr size_t MinimumQueuedSentences = 2;
static constexpr qint64 ReplayWindowMilliseconds = 2000;
static constexpr int SpeedRestartMilliseconds = 400;
static constexpr std::array<float, 5> SpeedSteps{0.5f, 0.75f, 1.0f, 1.25f, 1.5f};

ReaderReadAloud::ReaderReadAloud(const FTParagraphModel& InParagraphs, QObject* Parent)
    : QObject(Parent)
    , Paragraphs(InParagraphs)
{
    AppSpeech& Speech{AppSpeech::Get()};

    connect(&Speech, &AppSpeech::narrationStarted, this, &ReaderReadAloud::OnStarted);
    connect(&Speech, &AppSpeech::narrationFinished, this, &ReaderReadAloud::OnFinished);
    connect(&Speech, &AppSpeech::narrationFailed, this, &ReaderReadAloud::OnFailed);
    connect(&Speech, &AppSpeech::ready, this, &ReaderReadAloud::availableChanged);
    connect(&Speech, &AppSpeech::sentenceSpeedChanged, this, &ReaderReadAloud::OnSpeedChanged);
    connect(&AppCatalog::Get(), &AppCatalog::catalogChanged, this, &ReaderReadAloud::availableChanged);
    connect(&Speech, &AppSpeech::enabledChanged, this, [this]()
    {
        if (!IsAvailable())
        {
            stop();
        }

        emit availableChanged();
    });

    connect(&Speech, &AppSpeech::narrationPlayRequested, this, &ReaderReadAloud::resume);
    connect(&Speech, &AppSpeech::narrationPauseRequested, this, &ReaderReadAloud::pause);
    connect(&Speech, &AppSpeech::narrationStopRequested, this, &ReaderReadAloud::stop);

    connect(&Paragraphs, &FTParagraphModel::modelReset, this, &ReaderReadAloud::stop);
    connect(this, &ReaderReadAloud::stateChanged, this, &ReaderReadAloud::PublishSession);

    SpeedRestart.setSingleShot(true);
    SpeedRestart.setInterval(SpeedRestartMilliseconds);
    connect(&SpeedRestart, &QTimer::timeout, this, [this]()
    {
        if (bPlaying && Current.has_value())
        {
            qCInfo(LogSpeech) << "read aloud restarts the sentence at the new speed";

            PlayFrom(*Current);
        }
    });

#if defined(Q_OS_IOS)
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState State)
    {
        if (State != Qt::ApplicationActive && bPlaying)
        {
            qCInfo(LogSpeech) << "read aloud paused, the app left the foreground";

            pause();
        }
    });
#endif
}

void ReaderReadAloud::startAt(int Paragraph, int Character)
{
    const std::optional<FTNarrationSentence> Sentence{
        FindNarrationSentenceAt(Paragraphs.GetParagraphs(), Paragraph, Character, GetLanguageCode())};

    if (!Sentence.has_value())
    {
        qCInfo(LogSpeech) << "read aloud found nothing to read from paragraph" << Paragraph;

        return;
    }

    PlayFrom(*Sentence);
}

void ReaderReadAloud::pause()
{
    if (!bPlaying)
    {
        return;
    }

    qCInfo(LogSpeech) << "read aloud paused at paragraph" << GetParagraph();

    Silence();
    bPlaying = false;

    emit stateChanged();
}

void ReaderReadAloud::resume()
{
    if (bPlaying || !Current.has_value())
    {
        return;
    }

    PlayFrom(*Current);
}

void ReaderReadAloud::stop()
{
    if (!bPlaying && !Current.has_value())
    {
        return;
    }

    qCInfo(LogSpeech) << "read aloud stopped";

    Silence();
    bPlaying = false;
    Current.reset();

    emit stateChanged();
}

void ReaderReadAloud::previous()
{
    if (!Current.has_value())
    {
        return;
    }

    if (bPlaying && SentenceClock.isValid() && SentenceClock.elapsed() > ReplayWindowMilliseconds)
    {
        qCInfo(LogSpeech) << "read aloud replays the sentence";

        PlayFrom(*Current);

        return;
    }

    const std::optional<FTNarrationSentence> Before{
        FindNarrationSentenceBefore(Paragraphs.GetParagraphs(), *Current, GetLanguageCode())};

    qCInfo(LogSpeech) << "read aloud goes back a sentence";

    PlayFrom(Before.has_value() ? *Before : *Current);
}

void ReaderReadAloud::next()
{
    if (!Current.has_value())
    {
        return;
    }

    const std::optional<FTNarrationSentence> After{
        FindNarrationSentenceAfter(Paragraphs.GetParagraphs(), *Current, GetLanguageCode())};

    if (!After.has_value())
    {
        qCInfo(LogSpeech) << "read aloud has no sentence after this one";

        return;
    }

    qCInfo(LogSpeech) << "read aloud skips a sentence";

    PlayFrom(*After);
}

void ReaderReadAloud::stepSpeed()
{
    const float Speed{AppSpeech::Get().GetSentenceSpeed()};
    const auto Step{std::find_if(SpeedSteps.begin(), SpeedSteps.end(), [Speed](float Value) { return Value > Speed + 0.01f; })};

    SetSpeed(Step != SpeedSteps.end() ? *Step : SpeedSteps.front());
}

void ReaderReadAloud::enableSpeech()
{
    qCInfo(LogSpeech) << "read aloud turned speech on";

    AppSpeech::Get().SetEnabled(true);
}

void ReaderReadAloud::SetTitle(const QString& Value)
{
    Title = Value;
}

void ReaderReadAloud::SetSavedPlace(int Paragraph, int Character)
{
    if (SavedParagraph == Paragraph && SavedCharacter == Character)
    {
        return;
    }

    SavedParagraph = Paragraph;
    SavedCharacter = Character;

    emit savedChanged();
}

bool ReaderReadAloud::IsAvailable() const
{
    return AppSpeech::Get().CanNarrate();
}

bool ReaderReadAloud::IsVoiceInstalled() const
{
    return AppSpeech::Get().IsAvailable();
}

QString ReaderReadAloud::GetVoicePackage() const
{
    return AppCatalog::Get().FindVoicePackage(GetLanguageCode());
}

bool ReaderReadAloud::IsPlaying() const
{
    return bPlaying;
}

bool ReaderReadAloud::IsStarted() const
{
    return Current.has_value();
}

int ReaderReadAloud::GetParagraph() const
{
    return Current.has_value() ? Current->Paragraph : -1;
}

int ReaderReadAloud::GetSentenceStart() const
{
    return Current.has_value() ? static_cast<int>(Current->Start) : -1;
}

int ReaderReadAloud::GetSentenceEnd() const
{
    return Current.has_value() ? static_cast<int>(Current->End) : -1;
}

int ReaderReadAloud::GetSavedParagraph() const
{
    return SavedParagraph;
}

int ReaderReadAloud::GetSavedCharacter() const
{
    return SavedCharacter;
}

qreal ReaderReadAloud::GetSpeed() const
{
    return AppSpeech::Get().GetSentenceSpeed();
}

void ReaderReadAloud::SetSpeed(qreal Value)
{
    if (qFuzzyCompare(AppSpeech::Get().GetSentenceSpeed(), static_cast<float>(Value)))
    {
        return;
    }

    qCInfo(LogSpeech) << "read aloud speed ->" << Value;

    AppSpeech::Get().SetSentenceSpeed(static_cast<float>(Value));
}

qreal ReaderReadAloud::GetMinimumSpeed() const
{
    return MinimumSpeechSpeed;
}

qreal ReaderReadAloud::GetMaximumSpeed() const
{
    return MaximumSpeechSpeed;
}

void ReaderReadAloud::PlayFrom(const FTNarrationSentence& Sentence)
{
    qCInfo(LogSpeech) << "read aloud from paragraph" << Sentence.Paragraph << "character" << Sentence.Start;

    Silence();

    Current = Sentence;
    bPlaying = true;
    SentenceClock.invalidate();
    Remember(Sentence);

    QueueAhead();

    emit stateChanged();
}

void ReaderReadAloud::QueueAhead()
{
    double Seconds{0.0};

    for (const FQueuedSentence& Entry : Queued)
    {
        Seconds += Entry.Seconds;
    }

    const std::string LanguageCode{GetLanguageCode()};
    const float Speed{AppSpeech::Get().GetSpeed() * AppSpeech::Get().GetSentenceSpeed()};

    while (!bReachedEnd && (Queued.size() < MinimumQueuedSentences || Seconds < NarrationLookaheadSeconds))
    {
        const std::optional<FTNarrationSentence> Next{LastQueued.has_value()
            ? FindNarrationSentenceAfter(Paragraphs.GetParagraphs(), *LastQueued, LanguageCode)
            : Current};

        if (!Next.has_value())
        {
            bReachedEnd = true;

            break;
        }

        const FQueuedSentence Entry{NextId++, *Next, EstimateNarrationSeconds(Next->Text, Speed)};

        Queued.push_back(Entry);
        LastQueued = Next;
        Seconds += Entry.Seconds;

        AppSpeech::Get().QueueNarration(Entry.Id, Entry.Sentence.Text, QString::fromStdString(LanguageCode));
    }
}

void ReaderReadAloud::Silence()
{
    AppSpeech::Get().StopNarration();

    FirstLiveId = NextId;
    Queued.clear();
    LastQueued.reset();
    bReachedEnd = false;
}

void ReaderReadAloud::OnStarted(int Id)
{
    if (!bPlaying || Id < FirstLiveId)
    {
        return;
    }

    const auto Found{std::find_if(Queued.begin(), Queued.end(), [Id](const FQueuedSentence& Entry) { return Entry.Id == Id; })};

    if (Found == Queued.end())
    {
        return;
    }

    Current = Found->Sentence;
    SentenceClock.start();
    Remember(Found->Sentence);

    emit stateChanged();
}

void ReaderReadAloud::OnFinished(int Id)
{
    if (!bPlaying || Id < FirstLiveId)
    {
        return;
    }

    while (!Queued.empty() && Queued.front().Id <= Id)
    {
        Queued.pop_front();
    }

    QueueAhead();

    if (!Queued.empty())
    {
        return;
    }

    qCInfo(LogSpeech) << "read aloud reached the end of the book";

    Silence();
    bPlaying = false;
    Current.reset();

    emit stateChanged();
}

void ReaderReadAloud::OnFailed()
{
    if (!bPlaying)
    {
        return;
    }

    qCWarning(LogSpeech) << "read aloud stopped, the voice failed";

    stop();
}

void ReaderReadAloud::OnSpeedChanged()
{
    emit speedChanged();

    if (bPlaying)
    {
        SpeedRestart.start();
    }
}

void ReaderReadAloud::Remember(const FTNarrationSentence& Sentence)
{
    SetSavedPlace(Sentence.Paragraph, static_cast<int>(Sentence.Start));
}

void ReaderReadAloud::PublishSession()
{
    const bool bStarted{IsStarted()};

    if (bStarted == bPublishedStarted && bPlaying == bPublishedPlaying)
    {
        return;
    }

    bPublishedStarted = bStarted;
    bPublishedPlaying = bPlaying;

    AppSpeech::Get().UpdateNarrationSession(Title, bStarted, bPlaying);
}

std::string ReaderReadAloud::GetLanguageCode() const
{
    return FTSettings::Get().GetLanguageRules().TargetLanguage;
}
