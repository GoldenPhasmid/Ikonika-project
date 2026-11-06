#include "StageSessionControllerBase.h"

#include "AppSoundEffects.h"
#include "ExerciseFlowStages.h"
#include "MultipleChoiceController.h"
#include "SentenceTilesController.h"
#include "SessionSettings.h"
#include "SpeedMatchController.h"

#include "core/FCardSelectorBase.h"
#include "core/FTLog.h"

#include <algorithm>
#include <cstring>
#include <utility>

static constexpr const char* ChoiceStageKey = "choiceStage";
static constexpr const char* SentenceStageKey = "sentenceStage";
static constexpr const char* MatchStageKey = "matchStage";

StageSessionControllerBase::StageSessionControllerBase(QObject* Parent)
    : QObject(Parent)
{
}

void StageSessionControllerBase::completeStage()
{
    if (bFinished || GetCurrentStage() == nullptr)
    {
        return;
    }

    qCInfo(LogSession) << "User ended stage" << GetStage() << "early";

    if (GetCurrentStage()->SkipPhase())
    {
        emit stateChanged();

        return;
    }

    ForceCompleteCurrentStage();
    AdvanceToNextStage();

    emit stateChanged();
}

void StageSessionControllerBase::restartStage()
{
    if (bFinished || GetCurrentStage() == nullptr)
    {
        return;
    }

    qCInfo(LogSession) << "User restarted stage" << GetStage();

    ForceRestartCurrentStage();

    emit stateChanged();
}

bool StageSessionControllerBase::HasActiveSession() const
{
    return HasCards() && !bFinished;
}

bool StageSessionControllerBase::IsFinished() const
{
    return bFinished;
}

int StageSessionControllerBase::GetStage() const
{
    return HasCards() ? CurrentStageIndex + 1 : 0;
}

int StageSessionControllerBase::GetStageCount() const
{
    return HasCards() ? static_cast<int>(Stages.size()) : 0;
}

ExerciseControllerBase* StageSessionControllerBase::GetActiveController() const
{
    IExerciseFlowStage* Stage = GetCurrentStage();

    return Stage != nullptr ? Stage->GetActiveController() : nullptr;
}

void StageSessionControllerBase::ForceCompleteCurrentStage()
{
    GetCurrentStage()->ForceComplete();
}

void StageSessionControllerBase::ForceRestartCurrentStage()
{
    GetCurrentStage()->ForceRestart();
}

void StageSessionControllerBase::OnStageAdvance(int)
{
}

void StageSessionControllerBase::StartStages()
{
    CurrentStageIndex = 0;
    bFinished = false;
    bOutcomesApplied = false;

    BuildStages();

    Stages.front()->Start();

    emit stateChanged();
}

void StageSessionControllerBase::ResetStages()
{
    Stages.clear();
    MasteryStage = nullptr;
    MasteryStageIndex = -1;
    CurrentStageIndex = 0;
    bFinished = false;
    bOutcomesApplied = false;

    emit stateChanged();
}

void StageSessionControllerBase::AdvanceToNextStage()
{
    ++CurrentStageIndex;

    IExerciseFlowStage* Stage = GetCurrentStage();

    if (Stage == nullptr)
    {
        bFinished = true;

        if (!bOutcomesApplied)
        {
            bOutcomesApplied = true;
            ApplyOutcomes();

            AppSoundEffects::Get().PlayComplete();
        }

        return;
    }

    qCInfo(LogSession) << "Session moved to stage" << GetStage();

    Stage->Start();
}

void StageSessionControllerBase::OnActiveControllerStateChanged()
{
    IExerciseFlowStage* Stage = GetCurrentStage();

    if (Stage == nullptr)
    {
        return;
    }

    if (sender() != Stage->GetActiveController())
    {
        return;
    }

    Stage->OnControllerStateChanged();

    if (Stage->IsFinished())
    {
        OnStageAdvance(CurrentStageIndex);
        AdvanceToNextStage();
    }

    emit stateChanged();
}

IExerciseFlowStage* StageSessionControllerBase::GetCurrentStage() const
{
    if (CurrentStageIndex < 0 || CurrentStageIndex >= static_cast<int>(Stages.size()))
    {
        return nullptr;
    }

    return Stages[static_cast<size_t>(CurrentStageIndex)].get();
}

FSavedStagePosition StageSessionControllerBase::ResolveSavedStage(const QJsonObject& Root) const
{
    FSavedStagePosition Position;

    const int SavedIndex = Root["stageIndex"].toInt(0);
    int Shift = 0;

    for (const FOptionalStages& Optional : ListSavedStageLayout(Root))
    {
        const int SavedCount = ReadSavedStageCount(Root, Optional);
        const int SavedSlot = Optional.Slot - Shift;

        if (SavedIndex < SavedSlot)
        {
            break;
        }

        if (SavedCount != Optional.Count && SavedIndex < SavedSlot + SavedCount)
        {
            Position.StageIndex = Optional.ResumeIndex >= 0 ? Optional.ResumeIndex : Optional.Slot;
            Position.bRestoreExercise = false;

            qCInfo(LogSession) << metaObject()->className() << Optional.Key << "went from" << SavedCount << "to"
                               << Optional.Count << "stages since the session was saved, stage" << SavedIndex
                               << "resumes as" << Position.StageIndex << "from its start";

            return Position;
        }

        Shift += Optional.Count - SavedCount;
    }

    Position.StageIndex = SavedIndex + Shift;

    if (Shift != 0)
    {
        qCInfo(LogSession) << metaObject()->className() << "stages changed since the session was saved, stage"
                           << SavedIndex << "resumes as" << Position.StageIndex;
    }

    if (MasteryStage != nullptr && Position.StageIndex == MasteryStageIndex
        && Root["sentencePhase"].toInt(0) >= MasteryStage->GetPhaseCount())
    {
        ++Position.StageIndex;
        Position.bRestoreExercise = false;

        qCInfo(LogSession) << metaObject()->className() << "sentence phases are gone since the session was saved,"
                           << "the mastery stage counts as done";
    }

    return Position;
}

std::vector<FOptionalStages> StageSessionControllerBase::ListSavedStageLayout(const QJsonObject& Root) const
{
    std::vector<FOptionalStages> Layout = OptionalStages;

    if (MasteryStageIndex < 0 || Root.contains("sentencePhaseCount"))
    {
        return Layout;
    }

    int LegacyCount = 0;

    if (Root.contains("sentenceStageCount"))
    {
        LegacyCount = Root["sentenceStageCount"].toInt(0);
    }
    else if (Root.contains(SentenceStageKey))
    {
        LegacyCount = Root[SentenceStageKey].toBool() ? 1 : 0;
    }

    if (LegacyCount <= 0)
    {
        return Layout;
    }

    FOptionalStages Sentences;
    Sentences.Key = SentenceStageKey;
    Sentences.Slot = MasteryStageIndex + 1;
    Sentences.ResumeIndex = MasteryStageIndex;
    Sentences.SavedCountOverride = LegacyCount;

    const auto Position = std::find_if(Layout.begin(), Layout.end(),
                                       [&Sentences](const FOptionalStages& Optional) { return Optional.Slot >= Sentences.Slot; });
    Layout.insert(Position, Sentences);

    return Layout;
}

void StageSessionControllerBase::ResetOptionalStages()
{
    OptionalStages.clear();
    MasteryStage = nullptr;
    MasteryStageIndex = -1;
}

void StageSessionControllerBase::AppendChoiceStage(const FTSessionStartParams& Params, SessionSettings& Options,
                                                   std::vector<ExerciseControllerBase*>& OwnedControllers)
{
    if (!Options.GetRules().bChoiceStage)
    {
        qCInfo(LogSession) << metaObject()->className() << "multiple choice stage turned off in settings";

        RecordOptionalStages(ChoiceStageKey, &FTSessionExerciseRules::bChoiceStage, Options, 0, 1);

        return;
    }

    auto* ChoiceInstance = new MultipleChoiceController(this);
    ChoiceInstance->SetSessionSettings(&Options);

    AppendRoundsStage(ChoiceInstance, Options.GetRules().ChoiceCount, Params, OwnedControllers);
    RecordOptionalStages(ChoiceStageKey, &FTSessionExerciseRules::bChoiceStage, Options, 1, 1);
}

void StageSessionControllerBase::AppendMasteryStage(const FTSessionStartParams& Params, SessionSettings& Options,
                                                    std::vector<ExerciseControllerBase*>& OwnedControllers,
                                                    std::optional<FGapSentences> GapSentences)
{
    MasteryStageIndex = static_cast<int>(Stages.size());
    bBuiltWithSentenceStage = Options.GetRules().bSentenceStage;

    std::unique_ptr<FMasteryStage> Stage = FMasteryStage::Create(
        this, Params, Options, std::move(GapSentences), std::exchange(SavedSessionSentences, {}));
    MasteryStage = Stage.get();

    for (ExerciseControllerBase* Controller : MasteryStage->GetControllers())
    {
        OwnedControllers.push_back(Controller);
    }

    Stages.push_back(std::move(Stage));
}

void StageSessionControllerBase::AppendMatchStage(const FTSessionStartParams& Params, SessionSettings& Options,
                                                  std::vector<ExerciseControllerBase*>& OwnedControllers)
{
    if (!Options.GetRules().bMatchStage)
    {
        qCInfo(LogSession) << metaObject()->className() << "speed match stage turned off in settings";

        RecordOptionalStages(MatchStageKey, &FTSessionExerciseRules::bMatchStage, Options, 0, 1);

        return;
    }

    auto* MatchInstance = new SpeedMatchController(this);
    MatchInstance->SetSessionSettings(&Options);

    AppendRoundsStage(MatchInstance, Options.GetRules().MatchCount, Params, OwnedControllers);
    RecordOptionalStages(MatchStageKey, &FTSessionExerciseRules::bMatchStage, Options, 1, 1);
}

void StageSessionControllerBase::AppendRoundsStage(ExerciseControllerBase* Controller, int RoundsPerCard,
                                                   const FTSessionStartParams& Params,
                                                   std::vector<ExerciseControllerBase*>& OwnedControllers)
{
    FTSessionStartParams StageParams = Params;
    StageParams.FixedLength = Params.CardState->GetCount() * RoundsPerCard;
    StageParams.Selector = std::make_shared<FEachOnceSelector>(Params.CardState->GetCount(), true);

    OwnedControllers.push_back(Controller);
    Stages.push_back(std::make_unique<FProxyStage>(Controller, StageParams));
}

void StageSessionControllerBase::RecordOptionalStages(const char* Key, bool FTSessionExerciseRules::* Setting,
                                                      const SessionSettings& Options, int Count, int LegacyCount)
{
    OptionalStages.push_back(FOptionalStages{Key, Setting, Options.GetRules().*Setting,
                                             static_cast<int>(Stages.size()) - Count, Count, LegacyCount});
}

bool StageSessionControllerBase::HaveOptionalStagesChanged(const SessionSettings& Options) const
{
    if (MasteryStage != nullptr && Options.GetRules().bSentenceStage != bBuiltWithSentenceStage)
    {
        return true;
    }

    return std::any_of(OptionalStages.begin(), OptionalStages.end(), [&Options](const FOptionalStages& Optional)
    {
        return Options.GetRules().*Optional.Setting != Optional.bEnabled;
    });
}

int StageSessionControllerBase::ReadSavedStageCount(const QJsonObject& Root, const FOptionalStages& Optional)
{
    if (Optional.SavedCountOverride >= 0)
    {
        return Optional.SavedCountOverride;
    }

    const QString Key = QLatin1String(Optional.Key);
    const QString CountKey = Key + QStringLiteral("Count");

    if (Root.contains(CountKey))
    {
        return Root[CountKey].toInt(0);
    }

    if (Root.contains(Key))
    {
        return Root[Key].toBool() ? 1 : 0;
    }

    return Optional.LegacyCount;
}

void StageSessionControllerBase::SaveOptionalStages(QJsonObject& Json) const
{
    for (const FOptionalStages& Optional : OptionalStages)
    {
        const QString Key = QLatin1String(Optional.Key);

        Json[Key] = Optional.Count > 0;
        Json[Key + QStringLiteral("Count")] = Optional.Count;
    }

    if (MasteryStage == nullptr)
    {
        return;
    }

    Json["sentencePhaseCount"] = MasteryStage->GetPhaseCount() - 1;

    if (!MasteryStage->GetSentences().empty())
    {
        Json["sessionSentences"] = SessionSentencesToJson(MasteryStage->GetSentences());
    }

    if (GetCurrentStage() != MasteryStage)
    {
        return;
    }

    const int Phase = MasteryStage->GetPhase();

    Json["sentencePhase"] = Phase;

    if (Phase >= 1)
    {
        Json["sentenceStageProgress"] = MasteryStageToJson(MasteryStage->GetPhaseLoop(Phase));
    }
}

bool StageSessionControllerBase::RestoreSentencePhase(const QJsonObject& Root)
{
    const int Phase = Root["sentencePhase"].toInt(0);

    if (MasteryStage == nullptr || GetCurrentStage() != MasteryStage || Phase < 1 || Phase >= MasteryStage->GetPhaseCount())
    {
        return false;
    }

    auto ResolveSentenceCard = [this](const std::string& CardId) -> FTCardHandle
    {
        const std::vector<FSessionSentence>& Sentences = MasteryStage->GetSentences();
        const auto Found = std::find_if(Sentences.begin(), Sentences.end(),
                                        [&CardId](const FSessionSentence& Picked) { return Picked.Card->Id == CardId; });

        return Found != Sentences.end() ? Found->Card : nullptr;
    };

    MasteryStage->RestorePhase(Phase);

    FMasteryLoopStage& Loop = MasteryStage->GetPhaseLoop(Phase);

    if (!RestoreMasteryStage(Loop, Root["sentenceStageProgress"].toObject(), Root, ResolveSentenceCard))
    {
        qCWarning(LogSession) << metaObject()->className()
                              << "sentence phase has no saved batch that fits its sentences - a new batch starts";
    }

    qCInfo(LogSession) << metaObject()->className() << "sentence phase" << Phase << "restored -"
                       << Loop.SaveProgress().MasteryCount.size() << "sentences answered";

    return true;
}

QJsonObject StageSessionControllerBase::MasteryStageToJson(const FMasteryLoopStage& Stage)
{
    const FMasteryLoopProgress Progress = Stage.SaveProgress();

    QJsonObject Saved;
    Saved["masteryCount"] = IntCountsToJson(Progress.MasteryCount);
    Saved["batchCount"] = Progress.BatchCount;
    Saved["activeCandidateIndex"] = Stage.GetActiveCandidateIndex();
    Saved["activeBatchCardIds"] = CardIdsToJson(Stage.GetBatchCards());

    return Saved;
}

bool StageSessionControllerBase::RestoreMasteryStage(FMasteryLoopStage& Stage, const QJsonObject& Saved,
                                                     const QJsonObject& Root,
                                                     const std::function<FTCardHandle(const std::string&)>& ResolveCard)
{
    FMasteryLoopProgress Progress;
    Progress.MasteryCount = IntCountsFromJson(Saved["masteryCount"].toObject());
    Progress.BatchCount = Saved["batchCount"].toInt(0);

    FMasteryLoopActiveBatch ActiveBatch;
    const std::optional<std::vector<FTCardHandle>> BatchCards =
        CardIdsFromJson(Saved["activeBatchCardIds"].toArray(), ResolveCard);

    const bool bHasBatch = BatchCards.has_value() && !BatchCards->empty();

    if (bHasBatch)
    {
        ActiveBatch.CandidateIndex = Saved["activeCandidateIndex"].toInt(-1);
        ActiveBatch.BatchCards = *BatchCards;
        ActiveBatch.Checkpoint = ExerciseControllerBase::CheckpointFromJson(Root["activeExercise"].toObject());
    }

    Stage.RestoreProgress(Progress, ActiveBatch);

    return bHasBatch;
}

QJsonArray StageSessionControllerBase::CardIdsToJson(const std::vector<FTCardHandle>& Cards)
{
    QJsonArray Result;

    for (const FTCardHandle& Card : Cards)
    {
        Result.append(QString::fromStdString(Card->Id));
    }

    return Result;
}

std::optional<std::vector<FTCardHandle>> StageSessionControllerBase::CardIdsFromJson(
    const QJsonArray& Ids, const std::function<FTCardHandle(const std::string&)>& ResolveCard)
{
    std::vector<FTCardHandle> Result;
    Result.reserve(static_cast<size_t>(Ids.size()));

    for (const QJsonValue& Id : Ids)
    {
        FTCardHandle Card = ResolveCard(Id.toString().toStdString());

        if (!Card)
        {
            return std::nullopt;
        }

        Result.push_back(std::move(Card));
    }

    return Result;
}

QJsonObject StageSessionControllerBase::IntCountsToJson(const std::unordered_map<std::string, int>& Counts)
{
    QJsonObject Result;

    for (const auto& [CardId, Count] : Counts)
    {
        Result[QString::fromStdString(CardId)] = Count;
    }

    return Result;
}

std::unordered_map<std::string, int> StageSessionControllerBase::IntCountsFromJson(const QJsonObject& Counts)
{
    std::unordered_map<std::string, int> Result;

    for (auto It = Counts.constBegin(); It != Counts.constEnd(); ++It)
    {
        Result[It.key().toStdString()] = It.value().toInt();
    }

    return Result;
}
