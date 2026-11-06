#include "IntensiveLearningController.h"

#include "AppCards.h"
#include "AppDatabase.h"
#include "AppSessionCheckpoints.h"
#include "AppSpeech.h"
#include "AppState.h"
#include "AudioTranscriptionController.h"
#include "FormsStages.h"
#include "ExerciseFlowStages.h"
#include "SentenceTilesController.h"
#include "SessionSentenceStage.h"
#include "FlashcardController.h"
#include "FlipRecallController.h"
#include "WrittenProductionController.h"

#include "core/FTWordForms.h"
#include "core/FTCardStore.h"
#include "core/FTLog.h"
#include "core/FTSchema.h"
#include "core/FTSettings.h"

#include <QDateTime>
#include <QTimer>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <functional>

namespace
{
    constexpr const char* CheckpointKind = "Intensive";

    qint64 TakeSaveStamp()
    {
        static qint64 LastStamp = 0;

        LastStamp = std::max(LastStamp + 1, QDateTime::currentMSecsSinceEpoch());

        return LastStamp;
    }

    QJsonObject ReadCheckpoint(const QByteArray& Json)
    {
        return QJsonDocument::fromJson(Json).object();
    }

    QJsonObject FindParkedCheckpoint(const QString& SessionKey)
    {
        const auto Found = AppState::Get().IntensiveLearningCheckpoints.find(SessionKey);

        if (Found == AppState::Get().IntensiveLearningCheckpoints.end())
        {
            return {};
        }

        return ReadCheckpoint(Found->second);
    }

}

IntensiveLearningController::IntensiveLearningController(QObject* Parent)
    : StageSessionControllerBase(Parent)
{
    connect(this, &IntensiveLearningController::stateChanged,
            this, &IntensiveLearningController::SaveCheckpoint);

    QTimer::singleShot(0, this, &IntensiveLearningController::TryResumeCheckpoint);
}

bool IntensiveLearningController::startCards(const QString& InSessionKey, const QStringList& CardIds)
{
    if (HasActiveSession() && SessionKey == InSessionKey)
    {
        RebuildForChangedSettings();

        emit stateChanged();

        return true;
    }

    if (!AppDatabase::Get().IsReady() || CardIds.isEmpty())
    {
        return false;
    }

    FTCardRepository& CardRepository = AppDatabase::Get().GetCardRepository();

    const bool bFormsTraining = IsFormsTrainingId(InSessionKey.toStdString());

    std::vector<FTCardHandle> Cards;
    Cards.reserve(static_cast<size_t>(CardIds.size()));

    for (const QString& CardId : CardIds)
    {
        const std::optional<FTCardHandle> Card = AppCards::LoadSessionCard(CardRepository, CardId.toStdString());

        if (Card.has_value())
        {
            Cards.push_back(*Card);
        }
    }

    if (Cards.empty() || (bFormsTraining && AppCards::ExpandForms(Cards).empty()))
    {
        return false;
    }

    AppCards::Prepare(Cards);

    SaveCheckpoint();

    SessionKey = InSessionKey;
    BeginNewSession(std::make_shared<FCardState>(Cards), QString());

    return true;
}

void IntensiveLearningController::BeginNewSession(std::shared_ptr<FCardState> InCardState, const QString& InTitle)
{
    CardState = InCardState;
    Title = InTitle;
    CurrentStageIndex = 0;
    bFinished = false;
    bOutcomesApplied = false;
    FormsSentenceSeed = MakeRandomSeed();

    AccumulatedElapsedMs = 0;
    SessionTimer.restart();

    BuildStages();

    qCInfo(LogSession) << "Intensive learning session started on" << Title << "-" << CardState->GetCount()
                       << "cards, at stage 1";

    Stages.front()->Start();

    emit stateChanged();
}


bool IntensiveLearningController::resume(const QString& InSessionKey)
{
    if (InSessionKey.isEmpty())
    {
        return false;
    }

    if (HasActiveSession() && SessionKey == InSessionKey)
    {
        RebuildForChangedSettings();

        emit stateChanged();

        return true;
    }

    const auto Found = AppState::Get().IntensiveLearningCheckpoints.find(InSessionKey);

    if (Found == AppState::Get().IntensiveLearningCheckpoints.end())
    {
        return false;
    }

    SaveCheckpoint();

    const std::string Checkpoint = Found->second.toStdString();

    if (!LoadCheckpoint(Checkpoint))
    {
        qCWarning(LogSession) << "Dropped an intensive learning checkpoint for" << InSessionKey
                              << "- a card it referred to could not be resolved";

        AppState::Get().IntensiveLearningCheckpoints.erase(InSessionKey);
        AppSessionCheckpoints::Clear(CheckpointKind, InSessionKey.toStdString());

        return false;
    }

    return true;
}

bool IntensiveLearningController::IsActiveFor(const QString& InSessionKey) const
{
    return !InSessionKey.isEmpty() && HasActiveSession() && SessionKey == InSessionKey;
}

bool IntensiveLearningController::HasFormsRulesChanged(bool bSavedOptionalPronoun) const
{
    return IsFormsSession() && bSavedOptionalPronoun != FTSettings::Get().GetFormsRules().bOptionalPronoun;
}

void IntensiveLearningController::RebuildForChangedSettings()
{
    if (!HasFormsRulesChanged(bBuiltWithOptionalPronoun) && !HaveOptionalStagesChanged(SessionSettings::Training()))
    {
        return;
    }

    qCInfo(LogSession) << "Settings changed since the session was built - it rebuilds at stage" << GetStage();

    LoadCheckpoint(BuildCheckpointJson().toStdString());
}

bool IntensiveLearningController::hasSessionFor(const QString& InSessionKey) const
{
    return IsActiveFor(InSessionKey) || AppState::Get().IntensiveLearningCheckpoints.contains(InSessionKey);
}

QString IntensiveLearningController::lastActiveKey() const
{
    return HasActiveSession() ? SessionKey : QString();
}

int IntensiveLearningController::stageFor(const QString& InSessionKey) const
{
    if (IsActiveFor(InSessionKey))
    {
        return GetStage();
    }

    const QJsonObject Parked = FindParkedCheckpoint(InSessionKey);

    return Parked.isEmpty() ? 0 : Parked["stageIndex"].toInt(0) + 1;
}

int IntensiveLearningController::wordCountFor(const QString& InSessionKey) const
{
    if (IsActiveFor(InSessionKey))
    {
        return static_cast<int>(CardState->GetCount());
    }

    return static_cast<int>(FindParkedCheckpoint(InSessionKey)["cardIds"].toArray().size());
}

void IntensiveLearningController::restartSession()
{
    if (!HasActiveSession() || !AppDatabase::Get().IsReady())
    {
        return;
    }

    std::vector<FTCardHandle> Cards;
    Cards.reserve(CardState->GetCount());

    for (const FTCardHandle& Card : CardState->GetCards())
    {
        const std::optional<FTCardHandle> Reloaded = AppCards::LoadSessionCard(AppDatabase::Get().GetCardRepository(), Card->Id);

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

    qCInfo(LogSession) << "Intensive learning" << Title << "restarted";

    BeginNewSession(std::make_shared<FCardState>(Cards), Title);
}

void IntensiveLearningController::endSession()
{
    if (!HasActiveSession())
    {
        return;
    }

    qCInfo(LogSession) << "Intensive learning" << Title << "ended early at stage" << GetStage();

    DiscardSession();
}

QString IntensiveLearningController::GetTitle() const
{
    return Title;
}

QString IntensiveLearningController::GetVocabularyKey()
{
    return QStringLiteral("vocabulary");
}

bool IntensiveLearningController::HasCards() const
{
    return !SessionKey.isEmpty();
}

int IntensiveLearningController::GetElapsedSeconds() const
{
    const qint64 RunningMs = SessionTimer.isValid() ? SessionTimer.elapsed() : 0;

    return static_cast<int>((AccumulatedElapsedMs + RunningMs) / 1000);
}

int IntensiveLearningController::GetSessionWordCount() const
{
    return CardState != nullptr ? CardState->GetCount() : 0;
}

bool IntensiveLearningController::IsFormsSession() const
{
    return IsFormsTrainingId(SessionKey.toStdString());
}

bool IntensiveLearningController::removeCard(const QString& cardId)
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

void IntensiveLearningController::DiscardSession()
{
    AppSessionCheckpoints::Clear(CheckpointKind, SessionKey.toStdString());
    AppState::Get().IntensiveLearningCheckpoints.erase(SessionKey);

    SessionKey.clear();
    Title.clear();
    CardState = {};
    MixedStage = nullptr;
    FlipStage = nullptr;
    AccumulatedElapsedMs = 0;
    SessionTimer.invalidate();

    ResetStages();
    DestroyControllers();
}

void IntensiveLearningController::DestroyControllers()
{
    for (ExerciseControllerBase* Controller : OwnedControllers)
    {
        delete Controller;
    }

    OwnedControllers.clear();
}

void IntensiveLearningController::BuildStages()
{
    Stages.clear();
    DestroyControllers();
    MixedStage = nullptr;
    std::optional<FGapSentences> GapSentences = std::exchange(SavedGapSentences, std::nullopt);
    FlipStage = nullptr;
    ResetOptionalStages();
    bBuiltWithOptionalPronoun = FTSettings::Get().GetFormsRules().bOptionalPronoun;

    SessionSettings& Options = SessionSettings::Training();
    const FTSessionExerciseRules& Rules = Options.GetRules();

    FTSessionStartParams Params{};
    Params.CardState = CardState;
    Params.Title = Title;
    Params.TargetLanguage = FTSettings::Get().GetLanguageRules().TargetLanguage;
    Params.NativeLanguage = FTSettings::Get().GetLanguageRules().NativeLanguage;

    if (IsFormsSession())
    {
        FFormsStages Built = BuildFormsStages(this, Params, FormsSentenceSeed);

        Stages = std::move(Built.Stages);
        OwnedControllers = std::move(Built.Controllers);
        MixedStage = Built.TypingStage;

        ConnectStageControllers();

        return;
    }

    {
        FTSessionStartParams FlashcardParams = Params;
        FlashcardParams.FixedLength = Params.CardState->GetCount();
        FlashcardParams.Selector = std::make_shared<FEachOnceSelector>(Params.CardState->GetCount(), true);
        auto* FlashcardInstance = new FlashcardController(this);
        OwnedControllers.push_back(FlashcardInstance);
        Stages.push_back(std::make_unique<FProxyStage>(FlashcardInstance, FlashcardParams));
    }

    {
        FTSessionStartParams FlipParams = Params;
        FlipParams.FixedLength = FTSettings::Get().GetIntensiveLearningRules().CardsPerExercise;

        auto* FlipInstance = new FlipRecallController(this);
        OwnedControllers.push_back(FlipInstance);

        FlipInstance->SetQuestionSideOverride([](const ExerciseControllerBase& Controller) -> bool
        {
            const int Max = Controller.GetMaxSessionCount();

            if (Max <= 0)
            {
                return ExerciseControllerBase::RollShowsTargetWord(SHOW_RANDOM);
            }

            const int Third = (Controller.GetCurrentSessionCount() * 3) / Max;

            if (Third == 0)
            {
                return true;
            }

            if (Third == 1)
            {
                return ExerciseControllerBase::RollShowsTargetWord(SHOW_RANDOM);
            }

            return false;
        });

        std::vector<FMasteryStageSessionController> FlipCandidates{{FlipInstance, 1.0, [] { return true; }}};
        auto OwnedFlipStage = std::make_unique<FMasteryLoopStage>(
            std::move(FlipCandidates), std::vector<int>{}, FlipParams, Rules.FlipCount);
        FlipStage = OwnedFlipStage.get();
        Stages.push_back(std::move(OwnedFlipStage));
    }

    AppendChoiceStage(Params, Options, OwnedControllers);

    AppendMasteryStage(Params, Options, OwnedControllers, std::move(GapSentences));
    MixedStage = MasteryStage;

    AppendMatchStage(Params, Options, OwnedControllers);

    ConnectStageControllers();
}

void IntensiveLearningController::ConnectStageControllers()
{
    for (const std::unique_ptr<IExerciseFlowStage>& Stage : Stages)
    {
        for (ExerciseControllerBase* StageController : Stage->GetControllers())
        {
            connect(StageController, &ExerciseControllerBase::stateChanged, this, &IntensiveLearningController::OnActiveControllerStateChanged);
        }
    }
}

void IntensiveLearningController::ApplyOutcomes()
{
    std::vector<std::string> Promoted;
    Promoted.reserve(CardState->GetActiveCount());

    for (const FTCardHandle& Card : CardState->GetActiveCards())
    {
        Promoted.push_back(Card->Id);
    }

    FTCardRepository& CardRepository = AppDatabase::Get().GetCardRepository();

    const FTDayNumber Today = GetTodayDayNumber();
    const int PromotedCount = static_cast<int>(Promoted.size());

    if (IsFormsSession())
    {
        CardRepository.MarkFormsCardsLearned(Promoted, Today);

        qCInfo(LogSession) << "Forms training completed on" << SessionKey << "-" << PromotedCount << "words learned";
    }
    else
    {
        CardRepository.PromoteCards(Promoted);
        CardRepository.RecordDailyProgress(Today, PromotedCount, 0, FTSettings::Get().GetIntensiveLearningRules().GetDailyGoal());

        qCInfo(LogSession) << "Intensive learning session completed on" << Title << "-" << PromotedCount
                           << "cards promoted to box 1";
    }

    AppSessionCheckpoints::Clear(CheckpointKind, SessionKey.toStdString());
    AppState::Get().IntensiveLearningCheckpoints.erase(SessionKey);

    emit sessionEnded(SessionKey);
}

void IntensiveLearningController::SaveCheckpoint() const
{
    if (bResumingCheckpoint)
    {
        return;
    }
    if (bFinished || SessionKey.isEmpty() || CardState == nullptr)
    {
        return;
    }

    const QByteArray Bytes = BuildCheckpointJson();

    AppState::Get().IntensiveLearningCheckpoints[SessionKey] = Bytes;
    AppSessionCheckpoints::Save(CheckpointKind, SessionKey.toStdString(), Bytes.toStdString());
}

QByteArray IntensiveLearningController::BuildCheckpointJson() const
{
    QJsonObject Json;
    Json["sessionKey"] = SessionKey;
    Json["title"] = Title;
    Json["cardIds"] = CardIdsToJson(CardState->GetCards());
    Json["stageIndex"] = CurrentStageIndex;
    Json["formsSentenceSeed"] = static_cast<double>(FormsSentenceSeed);
    Json["optionalPronoun"] = bBuiltWithOptionalPronoun;
    Json["elapsedMs"] = AccumulatedElapsedMs + (SessionTimer.isValid() ? SessionTimer.elapsed() : 0);
    Json["savedAt"] = static_cast<double>(TakeSaveStamp());

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

        if (ActiveStage == static_cast<const IExerciseFlowStage*>(FlipStage))
        {
            Json["flipStageProgress"] = MasteryStageToJson(*FlipStage);
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

bool IntensiveLearningController::LoadCheckpoint(const std::string& Json)
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

    std::optional<std::vector<FTCardHandle>> RestoredCards =
        CardIdsFromJson(Root["cardIds"].toArray(), ResolveCard);

    if (!RestoredCards.has_value() || RestoredCards->empty())
    {
        return false;
    }

    bResumingCheckpoint = true;

    AppCards::Prepare(*RestoredCards);

    SessionKey = Root["sessionKey"].toString();
    Title = Root["title"].toString();
    CardState = std::make_shared<FCardState>(*RestoredCards);

    for (const QJsonValue& RemovedId : Root["removedCardIds"].toArray())
    {
        CardState->MarkRemoved(RemovedId.toString().toStdString());
    }

    CurrentStageIndex = 0;
    bFinished = false;
    bOutcomesApplied = false;
    FormsSentenceSeed = static_cast<uint32_t>(Root["formsSentenceSeed"].toDouble(0));

    AccumulatedElapsedMs = static_cast<qint64>(Root["elapsedMs"].toDouble(0));
    SessionTimer.restart();

    SavedSessionSentences = SessionSentencesFromJson(Root["sessionSentences"].toArray());
    SavedGapSentences = Root.contains("gapSentences") ? std::optional(GapSentencesFromJson(Root["gapSentences"].toObject()))
                                                     : std::nullopt;

    BuildStages();

    Stages.front()->Start();

    const FSavedStagePosition Saved = ResolveSavedStage(Root);

    while (CurrentStageIndex < Saved.StageIndex && !bFinished)
    {
        ForceCompleteCurrentStage();
        AdvanceToNextStage();
    }

    IExerciseFlowStage* ActiveStage = Saved.bRestoreExercise ? GetCurrentStage() : nullptr;

    const bool bFormsRulesChanged =
        HasFormsRulesChanged(Root["optionalPronoun"].toBool(FTSettings::Get().GetFormsRules().bOptionalPronoun));

    if (bFormsRulesChanged)
    {
        qCInfo(LogSession) << "Forms rules changed since the checkpoint was saved - stage" << GetStage()
                           << "starts a fresh exercise";
    }

    if (ActiveStage != nullptr && !RestoreSentencePhase(Root))
    {
        if (ActiveStage == static_cast<IExerciseFlowStage*>(FlipStage))
        {
            if (!RestoreMasteryStage(*FlipStage, Root["flipStageProgress"].toObject(), Root, ResolveCard))
            {
                qCInfo(LogSession) << "Checkpoint has no flip batch - the flip stage starts a fresh batch";
            }
        }
        else if (ActiveStage == static_cast<IExerciseFlowStage*>(MixedStage))
        {
            FMasteryLoopProgress Progress;
            Progress.MasteryCount = IntCountsFromJson(Root["masteryCount"].toObject());
            Progress.ConsecutiveWrongCount = IntCountsFromJson(Root["consecutiveWrongCount"].toObject());
            Progress.BatchCount = Root["batchCount"].toInt(0);

            FMasteryLoopActiveBatch ActiveBatch;
            const std::optional<std::vector<FTCardHandle>> RestoredBatchCards =
                CardIdsFromJson(Root["activeBatchCardIds"].toArray(), ResolveCard);

            const bool bBatchHasSkippedForm = RestoredBatchCards.has_value()
                && std::any_of(RestoredBatchCards->begin(), RestoredBatchCards->end(),
                               [](const FTCardHandle& Card) { return AppCards::IsFormSkipped(*Card); });

            if (bBatchHasSkippedForm)
            {
                qCInfo(LogSession) << "Checkpoint batch holds a form the settings leave out - a fresh batch starts";
            }

            if (!bFormsRulesChanged && !bBatchHasSkippedForm && RestoredBatchCards.has_value()
                && !RestoredBatchCards->empty())
            {
                ActiveBatch.CandidateIndex = Root["activeCandidateIndex"].toInt(-1);
                ActiveBatch.BatchCards = *RestoredBatchCards;
                ActiveBatch.Checkpoint = ExerciseControllerBase::CheckpointFromJson(Root["activeExercise"].toObject());
            }

            MixedStage->RestoreProgress(Progress, ActiveBatch);
        }
        else if (Root.contains("activeBatchCardIds"))
        {
            qCWarning(LogSession) << "Checkpoint exercise was saved by a mastery stage, now at stage" << GetStage()
                                  << "- the stage starts fresh";
        }
        else if (!bFormsRulesChanged)
        {
            ActiveStage->RestoreActiveExercise(ExerciseControllerBase::CheckpointFromJson(Root["activeExercise"].toObject()));
        }
    }

    bResumingCheckpoint = false;

    qCInfo(LogSession) << "Intensive learning session resumed from checkpoint on" << Title
                       << "at stage" << GetStage();

    emit stateChanged();

    return true;
}

void IntensiveLearningController::TryResumeCheckpoint()
{
    if (!AppDatabase::Get().IsReady())
    {
        return;
    }

    RestoreCheckpoints(AppSessionCheckpoints::LoadAll(CheckpointKind));
}

void IntensiveLearningController::RestoreCheckpoints(std::vector<std::pair<std::string, std::string>> Stored)
{
    const FScopedSpeechSilence Silence;

    std::erase_if(Stored, [](const std::pair<std::string, std::string>& Entry) { return Entry.first.empty(); });

    std::stable_sort(Stored.begin(), Stored.end(),
        [](const std::pair<std::string, std::string>& Left, const std::pair<std::string, std::string>& Right)
        {
            return ReadCheckpoint(QByteArray::fromStdString(Left.second))["savedAt"].toDouble(0)
                > ReadCheckpoint(QByteArray::fromStdString(Right.second))["savedAt"].toDouble(0);
        });

    for (const auto& [SessionKey, CheckpointJson] : Stored)
    {
        AppState::Get().IntensiveLearningCheckpoints[QString::fromStdString(SessionKey)] =
            QByteArray::fromStdString(CheckpointJson);
    }

    for (const auto& [SessionKey, CheckpointJson] : Stored)
    {
        if (LoadCheckpoint(CheckpointJson))
        {
            return;
        }

        qCWarning(LogSession) << "Dropped an intensive learning checkpoint for"
                              << QString::fromStdString(SessionKey) << "- a card it referred to"
                              << "could not be resolved";

        AppState::Get().IntensiveLearningCheckpoints.erase(QString::fromStdString(SessionKey));
        AppSessionCheckpoints::Clear(CheckpointKind, SessionKey);
    }
}
