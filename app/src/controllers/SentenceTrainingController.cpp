#include "SentenceTrainingController.h"

#include "AppDatabase.h"
#include "AppDictionary.h"
#include "AppReadSources.h"
#include "AppSessionCheckpoints.h"
#include "AppState.h"
#include "AudioTranscriptionController.h"
#include "ExerciseFlowStages.h"
#include "FlashcardController.h"
#include "SentenceSelection.h"
#include "SentenceTilesController.h"
#include "GapProductionController.h"

#include "core/FCardSelectorBase.h"
#include "core/FTLog.h"
#include "core/FTSentenceCards.h"
#include "core/FTSettings.h"
#include "core/FTWordForms.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QCoreApplication>
#include <QJsonObject>
#include <QPointer>
#include <QSettings>

#include <cstdint>
#include <QStringList>
#include <QThread>
#include <QThreadPool>
#include <QTimer>

#include <algorithm>
#include <map>

namespace
{
    const QString TrainingExercise{QStringLiteral("training")};
    const QString TilesExercise{QStringLiteral("tiles")};
    const QString MeaningExercise{QStringLiteral("meaning")};
    const QString GapExercise{QStringLiteral("gap")};
    const QString DictationExercise{QStringLiteral("dictation")};

    const QString DifficultyKey{QStringLiteral("sentences/difficulty")};
    const QString SentenceCountKey{QStringLiteral("sentences/count")};

    constexpr const char* CheckpointKind = "Sentences";

    bool IsKnownExercise(const QString& Exercise)
    {
        return Exercise == TrainingExercise || Exercise == TilesExercise || Exercise == MeaningExercise
            || Exercise == GapExercise || Exercise == DictationExercise;
    }

    int ClampDifficultyMask(int Mask)
    {
        const int Clamped = Mask & SentenceTrainingController::EveryDifficulty;

        return Clamped != 0 ? Clamped : SentenceTrainingController::EasyDifficulty;
    }

    int ClampSentenceCount(int Count)
    {
        const int Step = SentenceTrainingController::SentenceCountStep;
        const int Clamped = std::clamp(Count, SentenceTrainingController::MinimumSentenceCount,
                                       SentenceTrainingController::MaximumSentenceCount);

        return ((Clamped + Step / 2) / Step) * Step;
    }

    const char* FindUnusableReason(const FTTreeSentence& Sentence)
    {
        if (!HasGap(Sentence))
        {
            return "no gap";
        }

        return FindUnusableSentenceReason(Sentence);
    }

    QString JoinSentenceIds(const std::vector<FTTreeSentence>& Sentences)
    {
        QStringList Ids;
        Ids.reserve(static_cast<qsizetype>(Sentences.size()));

        for (const FTTreeSentence& Sentence : Sentences)
        {
            Ids.push_back(QString::number(Sentence.Id));
        }

        return Ids.join(QLatin1Char(','));
    }
}

SentenceTrainingController::SentenceTrainingController(QObject* Parent)
    : StageSessionControllerBase(Parent)
{
    const QSettings Settings(FTSettings::Get().OpenStore());

    DifficultyMask = ClampDifficultyMask(Settings.value(DifficultyKey, EasyDifficulty).toInt());
    SentenceCount = ClampSentenceCount(Settings.value(SentenceCountKey, DefaultSentenceCount).toInt());

    connect(this, &SentenceTrainingController::stateChanged,
            this, &SentenceTrainingController::SaveCheckpoint);

    QTimer::singleShot(0, this, &SentenceTrainingController::LoadStoredCheckpoints);
}

uint32_t SentenceTrainingController::MakeGrammarLevelMask(int InDifficultyMask)
{
    uint32_t Levels = 0;

    if ((InDifficultyMask & EasyDifficulty) != 0)
    {
        Levels |= MakeLevelMask(1);
    }

    if ((InDifficultyMask & NormalDifficulty) != 0)
    {
        Levels |= MakeLevelMask(2);
    }

    if ((InDifficultyMask & HardDifficulty) != 0)
    {
        Levels |= MakeLevelMask(3) | MakeLevelMask(4);
    }

    return Levels;
}

int SentenceTrainingController::ResolveDifficultyMask(int InDifficultyMask, int InUnlockedDifficultyMask)
{
    const int Unlocked = InDifficultyMask & InUnlockedDifficultyMask;

    if (Unlocked != 0)
    {
        return Unlocked;
    }

    for (const int Difficulty : {EasyDifficulty, NormalDifficulty, HardDifficulty})
    {
        if ((InUnlockedDifficultyMask & Difficulty) != 0)
        {
            return Difficulty;
        }
    }

    return InDifficultyMask;
}

int SentenceTrainingController::GetDifficultyMask() const
{
    return ResolveDifficultyMask(DifficultyMask, UnlockedDifficultyMask);
}

int SentenceTrainingController::GetUnlockedDifficultyMask() const
{
    return UnlockedDifficultyMask;
}

void SentenceTrainingController::SetDifficultyMask(int InDifficultyMask)
{
    const int Clamped = ClampDifficultyMask(InDifficultyMask);

    if (Clamped == DifficultyMask)
    {
        return;
    }

    DifficultyMask = Clamped;

    QSettings Settings(FTSettings::Get().OpenStore());
    Settings.setValue(DifficultyKey, DifficultyMask);

    qCInfo(LogSession) << "Sentence training: difficulty set to" << DifficultyMask;

    refresh();
}

int SentenceTrainingController::GetSentenceCount() const
{
    return SentenceCount;
}

void SentenceTrainingController::SetSentenceCount(int InSentenceCount)
{
    const int Clamped = ClampSentenceCount(InSentenceCount);

    if (Clamped == SentenceCount)
    {
        return;
    }

    SentenceCount = Clamped;

    QSettings Settings(FTSettings::Get().OpenStore());
    Settings.setValue(SentenceCountKey, SentenceCount);

    qCInfo(LogSession) << "Sentence training: session length set to" << SentenceCount << "sentences";

    refresh();
}

int SentenceTrainingController::GetMinimumSentenceCount() const
{
    return MinimumSentenceCount;
}

int SentenceTrainingController::GetMaximumSentenceCount() const
{
    return MaximumSentenceCount;
}

int SentenceTrainingController::GetSentenceCountStep() const
{
    return SentenceCountStep;
}

std::vector<FTTreeSentence> SentenceTrainingController::FindCandidates(int InDifficultyMask,
                                                                        int InSentenceCount) const
{
    if (!AppDatabase::Get().IsReady() || !AppDictionary::Get().IsReady())
    {
        return {};
    }

    const FTSentenceTree* Tree = AppDictionary::Get().GetSentenceTree();

    if (Tree == nullptr)
    {
        return {};
    }

    return FindCandidatesFor(*Tree, MakeSentenceSources(), InDifficultyMask, InSentenceCount);
}

std::vector<FTTreeSentence> SentenceTrainingController::FindCandidatesFor(const FTSentenceTree& Tree,
                                                                           const FSentenceSources& Sources,
                                                                           int InDifficultyMask, int InSentenceCount)
{
    const FTWordIdSet Known = BuildKnownSentenceWords(Tree, Sources);

    FTSentenceQuery Query;
    Query.MaxCount = static_cast<uint32_t>(InSentenceCount * CandidatesPerSentence);
    Query.LevelMask = MakeGrammarLevelMask(InDifficultyMask);
    Query.Seed = MakeRandomSeed();

    std::vector<FTTreeSentence> Candidates =
        Sources.Dictionary->LoadTreeSentences(Tree.FindWritableSentences(Known, Query));

    const size_t SampledCount = Candidates.size();

    std::map<std::string, int> UnusableCounts;

    Candidates.erase(std::remove_if(Candidates.begin(), Candidates.end(),
                                    [&UnusableCounts](const FTTreeSentence& Sentence)
                                    {
                                        const char* Reason = FindUnusableReason(Sentence);

                                        if (Reason == nullptr)
                                        {
                                            return false;
                                        }

                                        ++UnusableCounts[Reason];

                                        return true;
                                    }),
                     Candidates.end());

    QStringList UnusableSummary;

    for (const auto& [Reason, Count] : UnusableCounts)
    {
        UnusableSummary.push_back(QStringLiteral("%1 %2").arg(Count).arg(QString::fromStdString(Reason)));
    }

    qCInfo(LogSession) << "Sentence training:" << Known.GetCount() << "known words and verb tenses give" << Candidates.size()
                       << "usable sentences at difficulty" << InDifficultyMask << "- sampled" << SampledCount
                       << "of at most" << Query.MaxCount << (SampledCount < Query.MaxCount ? "(whole pool)" : "(capped)")
                       << "- rejected:" << (UnusableSummary.isEmpty() ? QStringLiteral("none") : UnusableSummary.join(QStringLiteral(", ")));

    return Candidates;
}

SentenceTrainingController::FAvailability SentenceTrainingController::ComputeAvailability(
    const FTSentenceTree& Tree, const FSentenceSources& Sources, int InDifficultyMask, int InSentenceCount)
{
    FAvailability Result;
    Result.bAvailable = true;

    for (const int Difficulty : {EasyDifficulty, NormalDifficulty, HardDifficulty})
    {
        const int DifficultyReadyCount =
            static_cast<int>(FindCandidatesFor(Tree, Sources, Difficulty, UnlockSentenceCount).size());

        if (DifficultyReadyCount >= UnlockSentenceCount)
        {
            Result.UnlockedDifficultyMask |= Difficulty;

            continue;
        }

        qCInfo(LogSession) << "Sentence training: difficulty" << Difficulty << "locked, has"
                           << DifficultyReadyCount << "sentences, needs" << UnlockSentenceCount;
    }

    const int Effective = ResolveDifficultyMask(InDifficultyMask, Result.UnlockedDifficultyMask);

    Result.ReadyCount = std::min(static_cast<int>(FindCandidatesFor(Tree, Sources, Effective, InSentenceCount).size()),
                                 InSentenceCount);

    return Result;
}

void SentenceTrainingController::ApplyAvailability(const FAvailability& Result, const std::string& Revision)
{
    bAvailable = Result.bAvailable;
    UnlockedDifficultyMask = Result.UnlockedDifficultyMask;
    bUnlocked = UnlockedDifficultyMask != 0;
    ReadyCount = Result.ReadyCount;
    RefreshedRevision = Revision;
    PendingRevision.clear();
    bRefreshing = false;

    emit availabilityChanged();
}

void SentenceTrainingController::refresh()
{
    const bool bReady = AppDictionary::Get().IsReady();

    const std::string Revision = bReady
        ? AppDatabase::Get().GetDataRevision() + "|"
          + std::to_string(AppDictionary::Get().GetGeneration()) + "|"
          + std::to_string(DifficultyMask) + "|" + std::to_string(SentenceCount)
        : std::string();

    if (!Revision.empty() && (Revision == RefreshedRevision || Revision == PendingRevision))
    {
        return;
    }

    ++RefreshSerial;

    if (!bReady)
    {
        ApplyAvailability({}, Revision);

        return;
    }

    const std::optional<FReadSourcesRequest> Request = AppReadSources::Capture();

    if (!Request.has_value())
    {
        const FTSentenceTree* Tree = AppDictionary::Get().GetSentenceTree();

        ApplyAvailability(Tree != nullptr ? ComputeAvailability(*Tree, MakeSentenceSources(), DifficultyMask, SentenceCount)
                                          : FAvailability{},
                          Revision);

        return;
    }

    PendingRevision = Revision;
    bRefreshing = true;

    qCInfo(LogSession) << "Sentence training: working out availability in the background";

    emit availabilityChanged();

    const quint64 Serial = RefreshSerial;
    const uint64_t Generation = AppDictionary::Get().GetGeneration();
    const std::shared_ptr<const FTSentenceTree> LoadedTree = AppDictionary::Get().GetLoadedSentenceTree();
    const std::vector<std::string> EssentialVerbs = FTSettings::Get().GetEssentialVerbs(Request->TargetLanguage);
    const int Mask = DifficultyMask;
    const int Count = SentenceCount;
    const QPointer<SentenceTrainingController> Self{this};

    QThreadPool::globalInstance()->start([Request = *Request, LoadedTree, EssentialVerbs, Mask, Count, Serial, Generation,
                                          Self]()
    {
        FAvailability Result;
        std::shared_ptr<const FTSentenceTree> Tree = LoadedTree;

        const std::unique_ptr<AppReadSources> Sources = AppReadSources::Open(Request);

        if (Sources != nullptr)
        {
            if (Tree == nullptr)
            {
                std::optional<FTSentenceTree> Loaded = FTSentenceTree::Load(Sources->GetDictionaryConnection());

                if (Loaded.has_value())
                {
                    Tree = std::make_shared<const FTSentenceTree>(std::move(*Loaded));
                }
            }

            if (Tree != nullptr)
            {
                const FSentenceSources Read{&Sources->GetDictionary(), &Sources->GetCards(), Request.TargetLanguage,
                                            EssentialVerbs};

                Result = ComputeAvailability(*Tree, Read, Mask, Count);
            }
        }

        const bool bOpened = Sources != nullptr;

        QMetaObject::invokeMethod(QCoreApplication::instance(),
                                  [Self, Result, Tree, Serial, Generation, bOpened]()
                                  {
                                      if (Self != nullptr)
                                      {
                                          Self->FinishRefresh(Serial, Generation, bOpened, Result, Tree);
                                      }
                                  },
                                  Qt::QueuedConnection);
    });
}

void SentenceTrainingController::FinishRefresh(quint64 Serial, uint64_t Generation, bool bOpened,
                                               const FAvailability& Result,
                                               const std::shared_ptr<const FTSentenceTree>& Tree)
{
    if (Serial != RefreshSerial)
    {
        return;
    }

    if (!bOpened)
    {
        qCWarning(LogSession) << "Sentence training: the background refresh could not open its connections";

        PendingRevision.clear();
        bRefreshing = false;

        emit availabilityChanged();

        return;
    }

    AppDictionary::Get().AdoptSentenceTree(Tree, Generation);

    ApplyAvailability(Result, PendingRevision);
}

bool SentenceTrainingController::IsRefreshing() const
{
    return bRefreshing;
}

void SentenceTrainingController::WaitForRefresh()
{
    while (bRefreshing)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(1);
    }
}

bool SentenceTrainingController::start(const QString& InExercise)
{
    if (!IsKnownExercise(InExercise))
    {
        qCWarning(LogSession) << "Sentence training: unknown exercise" << InExercise;

        return false;
    }

    if (HasActiveSession() && Exercise == InExercise)
    {
        qCInfo(LogSession) << "Sentence training: reopened the running" << InExercise
                           << "session at stage" << GetStage() << "- no new sentences picked";

        emit stateChanged();

        return true;
    }

    const auto Saved = AppState::Get().SentenceCheckpoints.find(InExercise);

    if (Saved != AppState::Get().SentenceCheckpoints.end())
    {
        qCInfo(LogSession) << "Sentence training: found a saved" << InExercise
                           << "session, trying to resume it instead of picking new sentences";

        const std::string Checkpoint = Saved->second.toStdString();

        if (LoadCheckpoint(Checkpoint))
        {
            return true;
        }

        qCWarning(LogSession) << "Sentence training: dropped the saved" << InExercise
                              << "session - it does not fit this dictionary or this session layout";

        ForgetCheckpoint(InExercise);
    }

    std::vector<FTTreeSentence> Candidates = FindCandidates(GetDifficultyMask(), SentenceCount);
    const size_t CandidateCount = Candidates.size();

    std::vector<FTTreeSentence> Picked = PickBalancedSentences(std::move(Candidates), SentenceCount,
                                                                   MakeRandomSeed());

    qCInfo(LogSession) << "Sentence training: picked" << Picked.size() << "of" << CandidateCount
                       << "candidates for" << InExercise << "- sentence ids" << JoinSentenceIds(Picked);

    if (static_cast<int>(Picked.size()) < SentenceCount)
    {
        qCInfo(LogSession) << "Sentence training: only" << Picked.size() << "sentences, needs"
                           << SentenceCount;

        return false;
    }

    SaveCheckpoint();
    ReleaseSession();

    std::vector<FTCardHandle> Cards;
    Cards.reserve(Picked.size());

    for (FTTreeSentence& Sentence : Picked)
    {
        FTCard Card = MakeSentenceCard(Sentence);

        Sentences.emplace(Card.Id, std::move(Sentence));
        Cards.push_back(MakeCardHandle(std::move(Card)));
    }

    Exercise = InExercise;
    CardState = std::make_shared<FCardState>(std::move(Cards));

    AccumulatedElapsedMs = 0;
    FinishedElapsedMs = 0;
    SessionTimer.restart();

    qCInfo(LogSession) << "Sentence training started:" << Exercise << "-" << CardState->GetCount()
                       << "sentences";

    StartStages();

    return true;
}

void SentenceTrainingController::endSession()
{
    if (!HasCards())
    {
        return;
    }

    qCInfo(LogSession) << "Sentence training ended at stage" << GetStage();

    DiscardSession();
}

void SentenceTrainingController::forgetSessions()
{
    DiscardSession();

    for (const QString& SavedExercise : GetSavedExercises())
    {
        ForgetCheckpoint(SavedExercise);
    }

    qCInfo(LogSession) << "Sentence training: every saved session dropped";

    emit stateChanged();
}

bool SentenceTrainingController::IsAvailable() const
{
    return bAvailable;
}

int SentenceTrainingController::GetReadyCount() const
{
    return ReadyCount;
}

bool SentenceTrainingController::CanStart() const
{
    return bAvailable && ReadyCount >= SentenceCount;
}

bool SentenceTrainingController::IsUnlocked() const
{
    return bUnlocked;
}

int SentenceTrainingController::GetElapsedSeconds() const
{
    if (bFinished)
    {
        return static_cast<int>(FinishedElapsedMs / 1000);
    }

    const qint64 RunningMs = SessionTimer.isValid() ? SessionTimer.elapsed() : 0;

    return static_cast<int>((AccumulatedElapsedMs + RunningMs) / 1000);
}

QStringList SentenceTrainingController::GetSavedExercises() const
{
    QStringList Result;

    for (const auto& [SavedExercise, Checkpoint] : AppState::Get().SentenceCheckpoints)
    {
        Result.push_back(SavedExercise);
    }

    return Result;
}

int SentenceTrainingController::GetSessionSentenceCount() const
{
    return CardState != nullptr ? CardState->GetCount() : 0;
}

bool SentenceTrainingController::HasCards() const
{
    return CardState != nullptr;
}

void SentenceTrainingController::AdoptController(ExerciseControllerBase* Controller)
{
    OwnedControllers.push_back(Controller);

    Controller->SetSpeaksSentences(true);

    connect(Controller, &ExerciseControllerBase::stateChanged, this,
            &SentenceTrainingController::OnActiveControllerStateChanged);
}

void SentenceTrainingController::AddFlashcardStage(const FTSessionStartParams& Params)
{
    FTSessionStartParams FlashcardParams = Params;
    FlashcardParams.Selector = std::make_shared<FEachOnceSelector>(CardState->GetCount(), true);

    auto* Flashcard = new FlashcardController(this);

    AdoptController(Flashcard);

    MasteryStages.push_back(nullptr);
    Stages.push_back(std::make_unique<FProxyStage>(Flashcard, FlashcardParams));
}

void SentenceTrainingController::AddStage(ExerciseControllerBase* Controller,
                                          const FTSessionStartParams& Params)
{
    AdoptController(Controller);

    std::vector<FMasteryStageSessionController> Candidates;
    Candidates.push_back(FMasteryStageSessionController{Controller, 1.0, [] { return true; }});

    auto Stage = std::make_unique<FMasteryLoopStage>(std::move(Candidates), std::vector<int>{0},
                                                     Params, 1);

    MasteryStages.push_back(Stage.get());
    Stages.push_back(std::move(Stage));
}

void SentenceTrainingController::BuildStages()
{
    Stages.clear();
    MasteryStages.clear();
    DestroyControllers();

    FTSessionStartParams Params{};
    Params.CardState = CardState;
    Params.Title = tr("Sentences");
    Params.TargetLanguage = FTSettings::Get().GetLanguageRules().TargetLanguage;
    Params.NativeLanguage = FTSettings::Get().GetLanguageRules().NativeLanguage;
    Params.FixedLength = CardState->GetCount();

    const bool bWholeSession = Exercise == TrainingExercise;

    if (bWholeSession)
    {
        AddFlashcardStage(Params);
    }

    if (bWholeSession || Exercise == TilesExercise)
    {
        AddStage(new SentenceTilesController(this), Params);
    }

    if (Exercise == MeaningExercise)
    {
        auto* Meaning = new SentenceTilesController(this);
        Meaning->SetBuildsTranslation(true);

        AddStage(Meaning, Params);
    }

    if (bWholeSession || Exercise == GapExercise)
    {
        auto* Gap = new SentenceGapController(this);
        Gap->SetSentenceLookup(
            [this](const FTCard& Card) -> const FTTreeSentence*
            {
                const auto Found = Sentences.find(Card.Id);

                return Found != Sentences.end() ? &Found->second : nullptr;
            });

        AddStage(Gap, Params);
    }

    if (bWholeSession || Exercise == DictationExercise)
    {
        auto* Dictation = new AudioTranscriptionController(this);
        Dictation->SetAnswerCheck(
            [Dictation](const std::string& Given, const std::string& Expected)
            {
                return IsCorrectGrade(CompareSentence(Given, Expected, Dictation->GetComparisonRules()));
            });

        AddStage(Dictation, Params);
    }
}

void SentenceTrainingController::ApplyOutcomes()
{
    FinishedElapsedMs = AccumulatedElapsedMs + (SessionTimer.isValid() ? SessionTimer.elapsed() : 0);

    qCInfo(LogSession) << "Sentence training completed:" << Exercise << "-" << GetSessionSentenceCount()
                       << "sentences in" << FinishedElapsedMs / 1000 << "seconds";

    ForgetCheckpoint(Exercise);
}

void SentenceTrainingController::DiscardSession()
{
    ForgetCheckpoint(Exercise);
    ReleaseSession();
}

void SentenceTrainingController::ReleaseSession()
{
    Exercise.clear();
    CardState = {};
    Sentences.clear();
    AccumulatedElapsedMs = 0;
    FinishedElapsedMs = 0;
    SessionTimer.invalidate();

    MasteryStages.clear();

    ResetStages();
    DestroyControllers();
}

void SentenceTrainingController::ForgetCheckpoint(const QString& InExercise)
{
    if (InExercise.isEmpty())
    {
        return;
    }

    AppSessionCheckpoints::Clear(CheckpointKind, InExercise.toStdString());
    AppState::Get().SentenceCheckpoints.erase(InExercise);
}

FMasteryLoopStage* SentenceTrainingController::GetCurrentMasteryStage() const
{
    const bool bValid = CurrentStageIndex >= 0
        && CurrentStageIndex < static_cast<int>(MasteryStages.size());

    return bValid ? MasteryStages[static_cast<size_t>(CurrentStageIndex)] : nullptr;
}

void SentenceTrainingController::SaveCheckpoint() const
{
    if (bResumingCheckpoint || bFinished || Exercise.isEmpty() || CardState == nullptr)
    {
        return;
    }

    const QByteArray Bytes = BuildCheckpointJson();

    AppState::Get().SentenceCheckpoints[Exercise] = Bytes;
    AppSessionCheckpoints::Save(CheckpointKind, Exercise.toStdString(), Bytes.toStdString());
}

QByteArray SentenceTrainingController::BuildCheckpointJson() const
{
    QJsonArray SavedSentences;

    for (const FTCardHandle& Card : CardState->GetCards())
    {
        const auto Found = Sentences.find(Card->Id);

        if (Found == Sentences.end())
        {
            continue;
        }

        QJsonObject Saved;
        Saved["id"] = static_cast<qint64>(Found->second.Id);
        Saved["text"] = QString::fromStdString(Found->second.Text);

        SavedSentences.append(Saved);
    }

    QJsonObject Json;
    Json["exercise"] = Exercise;
    Json["sentences"] = SavedSentences;
    Json["stageIndex"] = CurrentStageIndex;
    Json["stageCount"] = static_cast<int>(Stages.size());
    Json["elapsedMs"] = AccumulatedElapsedMs + (SessionTimer.isValid() ? SessionTimer.elapsed() : 0);

    if (const FMasteryLoopStage* Stage = GetCurrentMasteryStage())
    {
        const FMasteryLoopProgress Progress = Stage->SaveProgress();

        Json["masteryCount"] = IntCountsToJson(Progress.MasteryCount);
        Json["batchCount"] = Progress.BatchCount;
        Json["activeCandidateIndex"] = Stage->GetActiveCandidateIndex();

        QJsonArray BatchCardIds;

        for (const FTCardHandle& Card : Stage->GetBatchCards())
        {
            BatchCardIds.append(QString::fromStdString(Card->Id));
        }

        Json["activeBatchCardIds"] = BatchCardIds;
    }

    if (const ExerciseControllerBase* Active = GetActiveController())
    {
        if (const std::optional<FExerciseCheckpoint> Checkpoint = Active->TakeSession())
        {
            Json["activeExercise"] = ExerciseControllerBase::CheckpointToJson(*Checkpoint);
        }
    }

    return QJsonDocument(Json).toJson(QJsonDocument::Compact);
}

bool SentenceTrainingController::LoadCheckpoint(const std::string& Json)
{
    const QJsonDocument Document = QJsonDocument::fromJson(QByteArray::fromStdString(Json));

    if (!Document.isObject() || !AppDictionary::Get().IsReady())
    {
        qCWarning(LogSession) << "Sentence training: saved session rejected,"
                              << (Document.isObject() ? "the dictionary is not ready" : "it is not valid JSON");

        return false;
    }

    const QJsonObject Root = Document.object();
    const QString SavedExercise = Root["exercise"].toString();
    const QJsonArray SavedSentences = Root["sentences"].toArray();

    if (!IsKnownExercise(SavedExercise) || SavedSentences.isEmpty())
    {
        qCWarning(LogSession) << "Sentence training: saved session rejected, exercise" << SavedExercise
                              << "with" << SavedSentences.size() << "sentences is not usable";

        return false;
    }

    std::vector<uint32_t> SentenceIds;
    SentenceIds.reserve(static_cast<size_t>(SavedSentences.size()));

    for (const QJsonValue& Saved : SavedSentences)
    {
        SentenceIds.push_back(static_cast<uint32_t>(Saved.toObject()["id"].toInteger()));
    }

    std::unordered_map<uint32_t, FTTreeSentence> Loaded;

    for (FTTreeSentence& Sentence : AppDictionary::Get().GetRepository().LoadTreeSentences(SentenceIds))
    {
        const uint32_t SentenceId = Sentence.Id;

        Loaded.emplace(SentenceId, std::move(Sentence));
    }

    std::vector<FTTreeSentence> Restored;
    Restored.reserve(SentenceIds.size());

    for (const QJsonValue& Saved : SavedSentences)
    {
        const QJsonObject SavedSentence = Saved.toObject();
        const auto Found = Loaded.find(static_cast<uint32_t>(SavedSentence["id"].toInteger()));

        if (Found == Loaded.end() || Found->second.Text != SavedSentence["text"].toString().toStdString())
        {
            qCWarning(LogSession) << "Sentence training: saved session rejected, sentence"
                                  << SavedSentence["id"].toInteger()
                                  << (Found == Loaded.end() ? "is missing from this dictionary"
                                                            : "has different text in this dictionary");

            return false;
        }

        Restored.push_back(Found->second);
    }

    SaveCheckpoint();
    ReleaseSession();

    bResumingCheckpoint = true;

    std::vector<FTCardHandle> Cards;
    Cards.reserve(Restored.size());

    std::unordered_map<std::string, FTCardHandle> CardsById;

    for (FTTreeSentence& Sentence : Restored)
    {
        FTCard Card = MakeSentenceCard(Sentence);
        const std::string CardId = Card.Id;

        FTCardHandle Handle = MakeCardHandle(std::move(Card));

        Sentences.emplace(CardId, std::move(Sentence));
        CardsById.emplace(CardId, Handle);
        Cards.push_back(std::move(Handle));
    }

    Exercise = SavedExercise;
    CardState = std::make_shared<FCardState>(std::move(Cards));

    AccumulatedElapsedMs = static_cast<qint64>(Root["elapsedMs"].toDouble(0));
    FinishedElapsedMs = 0;
    SessionTimer.restart();

    BuildStages();

    const int StageCount = static_cast<int>(Stages.size());

    if (Root["stageCount"].toInt(0) != StageCount)
    {
        qCWarning(LogSession) << "Sentence training: saved session rejected, it has"
                              << Root["stageCount"].toInt(0) << "stages and this layout has" << StageCount;

        ReleaseSession();

        bResumingCheckpoint = false;

        return false;
    }

    CurrentStageIndex = std::clamp(Root["stageIndex"].toInt(0), 0, StageCount - 1);
    bFinished = false;
    bOutcomesApplied = false;

    std::optional<FExerciseCheckpoint> ActiveCheckpoint =
        ExerciseControllerBase::CheckpointFromJson(Root["activeExercise"].toObject());

    if (FMasteryLoopStage* CurrentMasteryStage = GetCurrentMasteryStage())
    {
        FMasteryLoopProgress Progress;
        Progress.MasteryCount = IntCountsFromJson(Root["masteryCount"].toObject());
        Progress.BatchCount = Root["batchCount"].toInt(0);

        FMasteryLoopActiveBatch ActiveBatch;
        ActiveBatch.CandidateIndex = Root["activeCandidateIndex"].toInt(-1);
        ActiveBatch.Checkpoint = std::move(ActiveCheckpoint);

        for (const QJsonValue& CardId : Root["activeBatchCardIds"].toArray())
        {
            const auto Found = CardsById.find(CardId.toString().toStdString());

            if (Found != CardsById.end())
            {
                ActiveBatch.BatchCards.push_back(Found->second);
            }
        }

        CurrentMasteryStage->RestoreProgress(Progress, ActiveBatch);
    }
    else if (ActiveCheckpoint.has_value())
    {
        GetCurrentStage()->RestoreActiveExercise(std::move(ActiveCheckpoint));
    }
    else
    {
        GetCurrentStage()->Start();
    }

    bResumingCheckpoint = false;

    qCInfo(LogSession) << "Sentence training resumed:" << Exercise << "at stage" << GetStage() << "-"
                       << Restored.size() << "sentences, ids" << JoinSentenceIds(Restored);

    emit stateChanged();

    return true;
}

void SentenceTrainingController::LoadStoredCheckpoints()
{
    const std::vector<std::pair<std::string, std::string>> Stored =
        AppSessionCheckpoints::LoadAll(CheckpointKind);

    if (Stored.empty())
    {
        return;
    }

    for (const auto& [ExerciseKey, CheckpointJson] : Stored)
    {
        AppState::Get().SentenceCheckpoints.try_emplace(QString::fromStdString(ExerciseKey),
                                                        QByteArray::fromStdString(CheckpointJson));
    }

    qCInfo(LogSession) << "Sentence training:" << Stored.size() << "saved sessions found";

    emit stateChanged();
}

void SentenceTrainingController::DestroyControllers()
{
    for (ExerciseControllerBase* Controller : OwnedControllers)
    {
        delete Controller;
    }

    OwnedControllers.clear();
}
