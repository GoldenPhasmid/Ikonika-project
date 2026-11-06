#include "RepetitionController.h"

#include "AppCards.h"
#include "AppDatabase.h"
#include "AppSessionCheckpoints.h"
#include "AppSpeech.h"
#include "AppState.h"
#include "AudioTranscriptionController.h"
#include "CorrectIncorrectController.h"
#include "ExerciseFlowStages.h"
#include "SentenceTilesController.h"
#include "SessionSentenceStage.h"
#include "WrittenProductionController.h"

#include "core/FTLog.h"
#include "core/FTRepetitionBatch.h"
#include "core/FTScheduleManager.h"
#include "core/FTSchema.h"
#include "core/FTSettings.h"
#include "core/sessions/FCardState.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QTimer>
#include <QJsonObject>

#include <algorithm>
#include <iterator>

namespace
{
    QString ReviewLabel()
    {
        return RepetitionController::tr("Review");
    }

    constexpr const char* CheckpointKind = "Repetition";

    QJsonObject BoolFlagsToJson(const std::unordered_map<std::string, bool>& Flags)
    {
        QJsonObject Result;

        for (const auto& [CardId, Flag] : Flags)
        {
            Result[QString::fromStdString(CardId)] = Flag;
        }

        return Result;
    }

    std::unordered_map<std::string, bool> BoolFlagsFromJson(const QJsonObject& Flags)
    {
        std::unordered_map<std::string, bool> Result;

        for (auto It = Flags.constBegin(); It != Flags.constEnd(); ++It)
        {
            Result[It.key().toStdString()] = It.value().toBool();
        }

        return Result;
    }

    constexpr int QuickPassFirstStage = 4;

    std::vector<FTCardHandle> TakeQuickPassCards(std::vector<FTCardHandle>& DueCards)
    {
        const auto FirstLate = std::stable_partition(DueCards.begin(), DueCards.end(),
            [](const FTCardHandle& Card) { return Card->LeitnerStage < QuickPassFirstStage; });

        std::vector<FTCardHandle> Late(std::make_move_iterator(FirstLate), std::make_move_iterator(DueCards.end()));
        DueCards.erase(FirstLate, DueCards.end());

        return Late;
    }

    QJsonArray StringSetToJson(const std::unordered_set<std::string>& Values)
    {
        QJsonArray Result;

        for (const std::string& Value : Values)
        {
            Result.append(QString::fromStdString(Value));
        }

        return Result;
    }

    std::unordered_set<std::string> StringSetFromJson(const QJsonArray& Values)
    {
        std::unordered_set<std::string> Result;

        for (const QJsonValue& Value : Values)
        {
            Result.insert(Value.toString().toStdString());
        }

        return Result;
    }
}

RepetitionController::RepetitionController(QObject* Parent)
    : StageSessionControllerBase(Parent)
{
    connect(this, &RepetitionController::stateChanged, this, &RepetitionController::SaveCheckpoint);

    QTimer::singleShot(0, this, &RepetitionController::TryResumeCheckpoint);
}

bool RepetitionController::startDueToday()
{
    if (HasActiveSession())
    {
        if (HaveOptionalStagesChanged(SessionSettings::Review()))
        {
            qCInfo(LogSession) << "Review stages changed in settings since the session was built - it rebuilds at stage"
                               << GetStage();

            LoadCheckpoint(BuildCheckpointJson().toStdString());
        }

        emit stateChanged();

        return true;
    }

    if (!AppState::Get().RepetitionCheckpoint.isEmpty())
    {
        const std::string Checkpoint = AppState::Get().RepetitionCheckpoint.toStdString();

        if (LoadCheckpoint(Checkpoint))
        {
            return true;
        }

        AppState::Get().RepetitionCheckpoint.clear();
    }

    if (!AppDatabase::Get().IsReady())
    {
        return false;
    }

    FTScheduleManager Schedule(AppDatabase::Get().GetCardRepository());

    const FTDayNumber Today = GetTodayDayNumber();
    std::vector<FTCardHandle> DueCards = Schedule.LoadDueCardsForToday(Today);

    AppCards::Prepare(DueCards);

    std::vector<FTCardHandle> QuickPassCards = TakeQuickPassCards(DueCards);

    if (!QuickPassCards.empty())
    {
        qCInfo(LogSession) << "Quick pass over" << QuickPassCards.size() << "cards from box" << QuickPassFirstStage
                           << "and up," << DueCards.size() << "earlier cards wait for the review after it";

        BeginNewSession(std::move(QuickPassCards), true, {});

        return true;
    }

    std::vector<std::vector<FTCardHandle>> Batches = SplitIntoRepetitionBatches(DueCards, MakeRandomSeed());

    if (Batches.empty())
    {
        qCInfo(LogSession) << "Nothing due today";

        return false;
    }

    BeginNewSession(std::move(Batches.front()), false, {});

    return true;
}

void RepetitionController::StartAfterQuickPass(const std::unordered_set<std::string>& Forgotten)
{
    if (!bFinished || !AppDatabase::Get().IsReady())
    {
        return;
    }

    FTScheduleManager Schedule(AppDatabase::Get().GetCardRepository());

    std::vector<FTCardHandle> DueCards = Schedule.LoadDueCardsForToday(GetTodayDayNumber());
    AppCards::Prepare(DueCards);

    std::vector<FTCardHandle> Cards;
    std::unordered_set<std::string> Carried;

    for (const FTCardHandle& Card : TakeQuickPassCards(DueCards))
    {
        if (Forgotten.count(Card->Id) > 0)
        {
            Cards.push_back(Card);
            Carried.insert(Card->Id);
        }
    }

    std::vector<std::vector<FTCardHandle>> Batches = SplitIntoRepetitionBatches(DueCards, MakeRandomSeed());

    if (!Batches.empty())
    {
        const size_t MaximumSize = static_cast<size_t>(GetMaximumRepetitionBatchSize());

        for (const FTCardHandle& Card : Batches.front())
        {
            if (Cards.size() >= MaximumSize)
            {
                break;
            }

            Cards.push_back(Card);
        }
    }

    if (Cards.empty())
    {
        qCInfo(LogSession) << "Quick pass left nothing to review today";

        emit sessionEnded();

        return;
    }

    qCInfo(LogSession) << "Review follows the quick pass -" << Carried.size() << "forgotten cards and"
                       << Cards.size() - Carried.size() << "earlier cards";

    BeginNewSession(std::move(Cards), false, std::move(Carried));
}

void RepetitionController::restartSession()
{
    if (!HasActiveSession() || !AppDatabase::Get().IsReady())
    {
        return;
    }

    std::vector<FTCardHandle> Cards;
    Cards.reserve(Batch->size());

    for (const FTCardHandle& Card : *Batch)
    {
        const std::optional<FTCardHandle> Reloaded = AppDatabase::Get().GetCardRepository().LoadCard(Card->Id);

        if (Reloaded.has_value())
        {
            Cards.push_back(*Reloaded);
        }
    }

    if (Cards.empty())
    {
        return;
    }

    AppCards::Prepare(Cards);

    qCInfo(LogSession) << "Repetition restarted";

    BeginNewSession(std::move(Cards), bQuickPass, CarriedCardIds);
}

void RepetitionController::endSession()
{
    if (!HasActiveSession())
    {
        return;
    }

    qCInfo(LogSession) << "Repetition ended early at stage" << GetStage();

    DiscardSession();
}

int RepetitionController::GetCardCount() const
{
    return static_cast<int>(Batch->size());
}

int RepetitionController::GetElapsedSeconds() const
{
    const qint64 RunningMs = SessionTimer.isValid() ? SessionTimer.elapsed() : 0;

    return static_cast<int>((AccumulatedElapsedMs + RunningMs) / 1000);
}

bool RepetitionController::IsQuickPass() const
{
    return bQuickPass;
}

bool RepetitionController::IsCarried(const std::string& CardId) const
{
    return CarriedCardIds.count(CardId) > 0;
}

bool RepetitionController::removeCard(const QString& cardId)
{
    if (CardState == nullptr)
    {
        return false;
    }

    ExerciseControllerBase* Active = GetActiveController();

    if (Active != nullptr && Active->removeCard(cardId))
    {
        return true;
    }

    return CardState->MarkRemoved(cardId.toStdString()) >= 0;
}

bool RepetitionController::HasCards() const
{
    return !Batch->empty();
}

void RepetitionController::OnCorrectIncorrectAnswered(const std::string& CardId, bool bPositive)
{
    ReviewedYes[CardId] = bPositive;
}

void RepetitionController::ForceCompleteCurrentStage()
{
    if (CurrentStageIndex == 0 && CardState != nullptr)
    {
        int Unanswered = 0;

        for (const FTCardHandle& Card : CardState->GetActiveCards())
        {
            if (!IsCarried(Card->Id) && ReviewedYes.emplace(Card->Id, true).second)
            {
                ++Unanswered;
            }
        }

        qCInfo(LogSession) << "Review stage completed early," << Unanswered << "unanswered cards count as correct";
    }

    StageSessionControllerBase::ForceCompleteCurrentStage();
}

void RepetitionController::DiscardSession()
{
    AppSessionCheckpoints::Clear(CheckpointKind, "");
    AppState::Get().RepetitionCheckpoint.clear();

    Batch->clear();
    MixedStage = nullptr;
    ReviewedYes.clear();
    CarriedCardIds.clear();
    bQuickPass = false;
    AccumulatedElapsedMs = 0;
    SessionTimer.invalidate();

    ResetStages();
    DestroyControllers();
}

void RepetitionController::DestroyControllers()
{
    for (ExerciseControllerBase* Controller : OwnedControllers)
    {
        delete Controller;
    }

    OwnedControllers.clear();
}

void RepetitionController::BeginNewSession(std::vector<FTCardHandle> Cards, bool bInQuickPass,
                                           std::unordered_set<std::string> InCarriedCardIds)
{
    *Batch = std::move(Cards);
    ReviewedYes.clear();
    bQuickPass = bInQuickPass;
    CarriedCardIds = std::move(InCarriedCardIds);

    AccumulatedElapsedMs = 0;
    SessionTimer.restart();

    StartStages();
    SkipEmptyReviewStage();

    qCInfo(LogSession) << (bQuickPass ? "Quick pass" : "Repetition session") << "started on" << Batch->size()
                       << "cards," << CarriedCardIds.size() << "of them forgotten in the quick pass";
}

void RepetitionController::SkipEmptyReviewStage()
{
    if (CurrentStageIndex != 0 || ReviewCardCount > 0 || bFinished)
    {
        return;
    }

    qCInfo(LogSession) << "Every card was forgotten in the quick pass - the review card stage is skipped";

    AdvanceToNextStage();

    emit stateChanged();
}

void RepetitionController::BuildStages()
{
    Stages.clear();
    DestroyControllers();
    MixedStage = nullptr;
    std::optional<FGapSentences> GapSentences = std::exchange(SavedGapSentences, std::nullopt);
    ResetOptionalStages();

    SessionSettings& Options = SessionSettings::Review();
    const QString Label = ReviewLabel();

    CardState = std::make_shared<FCardState>(*Batch);

    FTSessionStartParams Params{};
    Params.CardState = CardState;
    Params.Title = Label;
    Params.TargetLanguage = FTSettings::Get().GetLanguageRules().TargetLanguage;
    Params.NativeLanguage = FTSettings::Get().GetLanguageRules().NativeLanguage;

    {
        std::vector<int32> ReviewIndices;

        for (int Index = 0; Index < CardState->GetCount(); ++Index)
        {
            if (!IsCarried(CardState->GetCard(Index)->Id))
            {
                ReviewIndices.push_back(Index);
            }
        }

        ReviewCardCount = static_cast<int>(ReviewIndices.size());

        FTSessionStartParams ReviewParams = Params;
        ReviewParams.FixedLength = ReviewCardCount;
        ReviewParams.Selector = std::make_shared<FGroupedSelector>(
            std::vector<std::vector<int32>>{std::move(ReviewIndices)}, true, true);

        auto* ReviewInstance = new CorrectIncorrectController(this);
        OwnedControllers.push_back(ReviewInstance);
        connect(ReviewInstance, &CorrectIncorrectController::answerRecorded,
                this, &RepetitionController::OnCorrectIncorrectAnswered);
        Stages.push_back(std::make_unique<FProxyStage>(ReviewInstance, ReviewParams));
    }

    if (bQuickPass)
    {
        ConnectStageControllers();

        return;
    }

    AppendChoiceStage(Params, Options, OwnedControllers);

    AppendMasteryStage(Params, Options, OwnedControllers, std::move(GapSentences));
    MixedStage = MasteryStage;

    AppendMatchStage(Params, Options, OwnedControllers);

    ConnectStageControllers();
}

void RepetitionController::ConnectStageControllers()
{
    for (const std::unique_ptr<IExerciseFlowStage>& Stage : Stages)
    {
        for (ExerciseControllerBase* StageController : Stage->GetControllers())
        {
            connect(StageController, &ExerciseControllerBase::stateChanged,
                    this, &RepetitionController::OnActiveControllerStateChanged);
        }
    }
}

void RepetitionController::ApplyOutcomes()
{
    std::vector<FTRepetitionOutcome> Outcomes;
    Outcomes.reserve(Batch->size());

    const std::vector<std::string> RemovedIds = CardState->GetRemovedCardIds();
    const std::unordered_set<std::string> Removed(RemovedIds.begin(), RemovedIds.end());
    std::unordered_set<std::string> Forgotten;

    for (const FTCardHandle& Card : *Batch)
    {
        if (IsCarried(Card->Id))
        {
            if (Removed.count(Card->Id) == 0)
            {
                Outcomes.push_back(FTRepetitionOutcome{Card->Id, false});
            }

            continue;
        }

        const auto Found = ReviewedYes.find(Card->Id);

        if (Found == ReviewedYes.end())
        {
            continue;
        }

        if (bQuickPass && !Found->second)
        {
            Forgotten.insert(Card->Id);

            continue;
        }

        Outcomes.push_back(FTRepetitionOutcome{Card->Id, Found->second});
    }

    FTCardRepository& CardRepository = AppDatabase::Get().GetCardRepository();
    FTScheduleManager Schedule(CardRepository);

    const FTDayNumber Today = GetTodayDayNumber();
    const int Moved = Schedule.ReportSessionOutcomes(Outcomes, Today);

    CardRepository.RecordDailyProgress(Today, 0, static_cast<int>(Outcomes.size()),
                              FTSettings::Get().GetIntensiveLearningRules().GetDailyGoal());

    qCInfo(LogSession) << (bQuickPass ? "Quick pass" : "Repetition") << "completed -" << Outcomes.size()
                       << "cards answered," << Moved << "moved," << Forgotten.size() << "forgotten";

    AppSessionCheckpoints::Clear(CheckpointKind, "");
    AppState::Get().RepetitionCheckpoint.clear();

    if (bQuickPass)
    {
        QTimer::singleShot(0, this, [this, Forgotten] { StartAfterQuickPass(Forgotten); });

        return;
    }

    emit sessionEnded();
}

void RepetitionController::SaveCheckpoint() const
{
    if (bResumingCheckpoint)
    {
        return;
    }

    if (bFinished || Batch->empty())
    {
        AppSessionCheckpoints::Clear(CheckpointKind, "");
        AppState::Get().RepetitionCheckpoint.clear();

        return;
    }

    const QByteArray Bytes = BuildCheckpointJson();

    AppState::Get().RepetitionCheckpoint = Bytes;
    AppSessionCheckpoints::Save(CheckpointKind, "", Bytes.toStdString());
}

QByteArray RepetitionController::BuildCheckpointJson() const
{
    QJsonObject Json;
    Json["cardIds"] = CardIdsToJson(*Batch);
    Json["stageIndex"] = CurrentStageIndex;
    Json["reviewedYes"] = BoolFlagsToJson(ReviewedYes);
    Json["quickPass"] = bQuickPass;
    Json["carriedCardIds"] = StringSetToJson(CarriedCardIds);
    Json["elapsedMs"] = AccumulatedElapsedMs + (SessionTimer.isValid() ? SessionTimer.elapsed() : 0);

    QJsonArray RemovedCardIds;

    for (const std::string& RemovedId : CardState->GetRemovedCardIds())
    {
        RemovedCardIds.append(QString::fromStdString(RemovedId));
    }

    Json["removedCardIds"] = RemovedCardIds;

    SaveOptionalStages(Json);

    if (MixedStage != nullptr)
    {
        const FMasteryLoopProgress Progress = MixedStage->SaveProgress();

        Json["gapSentences"] = GapSentencesToJson(MixedStage->GetGapSentences());

        Json["masteryCount"] = IntCountsToJson(Progress.MasteryCount);
        Json["consecutiveWrongCount"] = IntCountsToJson(Progress.ConsecutiveWrongCount);
        Json["batchCount"] = Progress.BatchCount;
    }

    if (const IExerciseFlowStage* ActiveStage = GetCurrentStage())
    {
        const bool bInWordPhase = MasteryStage == nullptr || MasteryStage->GetPhase() == 0;

        if (ActiveStage == static_cast<const IExerciseFlowStage*>(MixedStage) && bInWordPhase)
        {
            Json["activeCandidateIndex"] = MixedStage->GetActiveCandidateIndex();
            Json["activeBatchCardIds"] = CardIdsToJson(MixedStage->GetBatchCards());
        }

        if (const ExerciseControllerBase* Active = ActiveStage->GetActiveController())
        {
            if (const std::optional<FExerciseCheckpoint> Checkpoint = Active->TakeSession())
            {
                Json["activeExercise"] = ExerciseControllerBase::CheckpointToJson(*Checkpoint);
            }
        }
    }

    return QJsonDocument(Json).toJson(QJsonDocument::Compact);
}

bool RepetitionController::LoadCheckpoint(const std::string& Json)
{
    const QJsonDocument Document = QJsonDocument::fromJson(QByteArray::fromStdString(Json));

    if (!Document.isObject())
    {
        return false;
    }

    const QJsonObject Root = Document.object();

    auto ResolveCard = [](const std::string& CardId) -> FTCardHandle
    {
        const std::optional<FTCardHandle> Card = AppCards::LoadSessionCard(AppDatabase::Get().GetCardRepository(), CardId);

        return Card.has_value() ? *Card : nullptr;
    };

    std::optional<std::vector<FTCardHandle>> RestoredBatch =
        CardIdsFromJson(Root["cardIds"].toArray(), ResolveCard);

    if (!RestoredBatch.has_value() || RestoredBatch->empty())
    {
        return false;
    }

    bResumingCheckpoint = true;

    AppCards::Prepare(*RestoredBatch);

    *Batch = std::move(*RestoredBatch);
    ReviewedYes = BoolFlagsFromJson(Root["reviewedYes"].toObject());
    bQuickPass = Root["quickPass"].toBool(false);
    CarriedCardIds = StringSetFromJson(Root["carriedCardIds"].toArray());

    AccumulatedElapsedMs = static_cast<qint64>(Root["elapsedMs"].toDouble(0));
    SessionTimer.restart();

    SavedSessionSentences = SessionSentencesFromJson(Root["sessionSentences"].toArray());
    SavedGapSentences = Root.contains("gapSentences") ? std::optional(GapSentencesFromJson(Root["gapSentences"].toObject()))
                                                     : std::nullopt;

    StartStages();

    for (const QJsonValue& RemovedId : Root["removedCardIds"].toArray())
    {
        CardState->MarkRemoved(RemovedId.toString().toStdString());
    }

    const FSavedStagePosition Saved = ResolveSavedStage(Root);

    while (CurrentStageIndex < Saved.StageIndex && !bFinished)
    {
        ForceCompleteCurrentStage();
        AdvanceToNextStage();
    }

    IExerciseFlowStage* ActiveStage = Saved.bRestoreExercise ? GetCurrentStage() : nullptr;

    if (ActiveStage != nullptr && !RestoreSentencePhase(Root))
    {
        if (ActiveStage == static_cast<IExerciseFlowStage*>(MixedStage))
        {
            FMasteryLoopProgress Progress;
            Progress.MasteryCount = IntCountsFromJson(Root["masteryCount"].toObject());
            Progress.ConsecutiveWrongCount = IntCountsFromJson(Root["consecutiveWrongCount"].toObject());
            Progress.BatchCount = Root["batchCount"].toInt(0);

            FMasteryLoopActiveBatch ActiveBatch;
            const std::optional<std::vector<FTCardHandle>> RestoredBatchCards =
                CardIdsFromJson(Root["activeBatchCardIds"].toArray(), ResolveCard);

            if (RestoredBatchCards.has_value() && !RestoredBatchCards->empty())
            {
                ActiveBatch.CandidateIndex = Root["activeCandidateIndex"].toInt(-1);
                ActiveBatch.BatchCards = *RestoredBatchCards;
                ActiveBatch.Checkpoint = ExerciseControllerBase::CheckpointFromJson(Root["activeExercise"].toObject());
            }

            MixedStage->RestoreProgress(Progress, ActiveBatch);
        }
        else
        {
            ActiveStage->RestoreActiveExercise(ExerciseControllerBase::CheckpointFromJson(Root["activeExercise"].toObject()));
        }
    }

    bResumingCheckpoint = false;

    SkipEmptyReviewStage();

    qCInfo(LogSession) << (bQuickPass ? "Quick pass" : "Repetition session") << "resumed from checkpoint at stage"
                       << GetStage();

    emit stateChanged();

    return true;
}

void RepetitionController::TryResumeCheckpoint()
{
    if (!AppDatabase::Get().IsReady())
    {
        return;
    }

    const FScopedSpeechSilence Silence;

    for (const auto& [SessionKey, CheckpointJson] : AppSessionCheckpoints::LoadAll(CheckpointKind))
    {
        if (!LoadCheckpoint(CheckpointJson))
        {
            qCWarning(LogSession) << "Dropped a repetition checkpoint - a card it referred to"
                                  << "could not be resolved";

            AppSessionCheckpoints::Clear(CheckpointKind, SessionKey);
        }

        break;
    }
}
