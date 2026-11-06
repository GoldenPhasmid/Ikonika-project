#include "ExerciseFlowStages.h"

#include <QCoreApplication>

#include <algorithm>
#include <iterator>
#include <utility>

#include "AudioTranscriptionController.h"
#include "SentenceTilesController.h"
#include "SessionSentenceStage.h"
#include "SessionSettings.h"
#include "WrittenProductionController.h"
#include "core/FTLog.h"
#include "core/FTSentenceCards.h"
#include "core/FTSettings.h"
#include "core/sessions/FCardState.h"

static FWrittenContent MakeSentenceGapContent(const FTCard& Card, const FTTreeSentence& Sentence,
                                              const std::string& NativeLanguage)
{
    FWrittenContent Content;
    Content.Heading = Sentence.Translation;
    Content.Prompt = MakeGapPrompt(Sentence);
    Content.AcceptedAnswers = {GetGapWord(Sentence)};
    Content.Translation = GetDisplayTranslation(Card, NativeLanguage);
    Content.RevealedAnswer = Sentence.Text;
    Content.RevealedSpeech = GetGapWord(Sentence);
    Content.bSentence = true;

    return Content;
}

FProxyStage::FProxyStage(ExerciseControllerBase* InController, const FTSessionStartParams& InParams)
    : Controller(InController)
    , Params(InParams)
{

}

ExerciseControllerBase* FProxyStage::GetActiveController() const
{
    return Controller;
}

std::vector<ExerciseControllerBase*> FProxyStage::GetControllers() const
{
    return {Controller};
}

void FProxyStage::Start()
{
    bForceFinished = false;

    Controller->Start(Params);
}

void FProxyStage::OnControllerStateChanged()
{
}

bool FProxyStage::IsFinished() const
{
    return bForceFinished || Controller->IsFinished();
}

void FProxyStage::ForceComplete()
{
    bForceFinished = true;
}

void FProxyStage::ForceRestart()
{
    Start();
}

void FProxyStage::RestoreActiveExercise(std::optional<FExerciseCheckpoint> Checkpoint)
{
    if (!Checkpoint.has_value())
    {
        return;
    }

    bForceFinished = false;

    Controller->RestoreSession(Params, std::move(Checkpoint));
}

const FGapSentences& FMasteryLoopStage::GetGapSentences() const
{
    return *GapSentences;
}

void FMasteryLoopStage::SetGapSentences(std::shared_ptr<const FGapSentences> InGapSentences)
{
    GapSentences = std::move(InGapSentences);
}

const std::vector<FTCardHandle>& FMasteryLoopStage::GetCards() const
{
    return Params.CardState->GetCards();
}

FUnmasteredSelector::FUnmasteredSelector(int32 InCount, std::function<bool(int32)> InIsMastered)
    : FEachOnceSelector(InCount, false)
    , IsMastered(std::move(InIsMastered))
{
}

std::optional<int> FUnmasteredSelector::Next()
{
    for (;;)
    {
        const std::optional<int> Index = FEachOnceSelector::Next();

        if (!Index.has_value() || !IsMastered(*Index))
        {
            return Index;
        }
    }
}

FMasteryLoopStage::FMasteryLoopStage(std::vector<FMasteryStageSessionController> InCandidates, std::vector<int> InOrder,
    FTSessionStartParams InParams, int InRepeatsToMaster)
    : Candidates(std::move(InCandidates))
    , InitialOrder(std::move(InOrder))
    , Params(std::move(InParams))
    , RepeatsToMaster(InRepeatsToMaster)
{
    for (const auto& Entry: Candidates)
    {
        Entry.Controller->SetProgressPolicy(this);
    }
}

void FMasteryLoopStage::SetCardGroup(FCardGroup InCardGroup)
{
    CardGroup = std::move(InCardGroup);
}

void FMasteryLoopStage::SetBatchGroups(std::vector<std::vector<int>> InBatchGroups)
{
    BatchGroups = std::move(InBatchGroups);
}

int FMasteryLoopStage::GetCurrent()
{
    return GetEarnedAnswers();
}

int FMasteryLoopStage::GetMax()
{
    return RepeatsToMaster * GetActiveCount();
}

float FMasteryLoopStage::GetProgress()
{
    const int Target = GetMax();
    if (Target <= 0)
    {
        return 0.0f;
    }

    return static_cast<float>(GetEarnedAnswers()) / static_cast<float>(Target);
}

int FMasteryLoopStage::GetMasteryTarget()
{
    return RepeatsToMaster;
}

int FMasteryLoopStage::GetCardMasteryCount(const std::string& CardId)
{
    const auto Found = MasteryCount.find(CardId);

    return Found != MasteryCount.end() ? std::min(Found->second, RepeatsToMaster) : 0;
}

ExerciseControllerBase* FMasteryLoopStage::GetActiveController() const
{
    return ActiveController;
}

std::vector<ExerciseControllerBase*> FMasteryLoopStage::GetControllers() const
{
    std::vector<ExerciseControllerBase*> Result;
    Result.reserve(Candidates.size());

    for (const FMasteryStageSessionController& Candidate : Candidates)
    {
        Result.push_back(Candidate.Controller);
    }

    return Result;
}

void FMasteryLoopStage::Start()
{
    StartNextBatch();
}

void FMasteryLoopStage::OnControllerStateChanged()
{
    if (ActiveController == nullptr)
    {
        return;
    }

    if (ActiveController->IsFinished())
    {
        ++BatchCount;

        if (!IsLoopFinished())
        {
            StartNextBatch();
        }
    }
}

bool FMasteryLoopStage::IsFinished() const
{
    return IsLoopFinished();
}

bool FMasteryLoopStage::IsLoopFinished() const
{
    if (ActiveController != nullptr && ActiveController->IsAnswered())
    {
        return false;
    }

    const int ActiveCount = GetActiveCount();

    if (ActiveCount <= 0)
    {
        return true;
    }

    return GetMasteredCount() >= ActiveCount;
}

void FMasteryLoopStage::ForceComplete()
{
    for (const FTCardHandle& Card : Params.CardState->GetCards())
    {
        MasteryCount[Card->Id] = RepeatsToMaster;
    }
}

void FMasteryLoopStage::ForceRestart()
{
    MasteryCount.clear();
    BatchCount = 0;

    Start();
}

FMasteryLoopProgress FMasteryLoopStage::SaveProgress() const
{
    FMasteryLoopProgress Progress;
    Progress.MasteryCount = MasteryCount;
    Progress.BatchCount = BatchCount;

    return Progress;
}

void FMasteryLoopStage::RestoreProgress(const FMasteryLoopProgress& Progress, const FMasteryLoopActiveBatch& ActiveBatch)
{
    MasteryCount = Progress.MasteryCount;
    BatchCount = Progress.BatchCount;

    const bool bHasActiveBatch = ActiveBatch.CandidateIndex >= 0
        && ActiveBatch.CandidateIndex < static_cast<int>(Candidates.size())
        && !ActiveBatch.BatchCards.empty()
        && ActiveBatch.Checkpoint.has_value();

    if (!bHasActiveBatch)
    {
        StartNextBatch();
        return;
    }

    BatchCards = ActiveBatch.BatchCards;
    ActiveController = Candidates[static_cast<size_t>(ActiveBatch.CandidateIndex)].Controller;

    FTSessionStartParams ExerciseParams = Params;
    ExerciseParams.CardState = std::make_shared<FCardState>(BatchCards);
    ExerciseParams.Selector = MakeBatchSelector();

    ActiveController->RestoreSession(ExerciseParams, ActiveBatch.Checkpoint);
}

int FMasteryLoopStage::GetActiveCandidateIndex() const
{
    for (size_t Index = 0; Index < Candidates.size(); ++Index)
    {
        if (Candidates[Index].Controller == ActiveController)
        {
            return static_cast<int>(Index);
        }
    }

    return -1;
}

int FMasteryLoopStage::GetActiveCount() const
{
    return Params.CardState->GetCount();
}

int FMasteryLoopStage::GetMasteredCount() const
{
    int Count = 0;

    for (const FTCardHandle& Card : Params.CardState->GetCards())
    {
        if (IsMastered(Card->Id))
        {
            ++Count;
        }
    }

    return Count;
}

int FMasteryLoopStage::GetEarnedAnswers()
{
    int Sum = 0;

    for (const FTCardHandle& Card : Params.CardState->GetCards())
    {
        Sum += GetCardMasteryCount(Card->Id);
    }

    return Sum;
}

void FMasteryLoopStage::OnAnswerRecorded(const std::string& CardId, bool bCorrect)
{
    MasteryCount[CardId] += bCorrect ? 1 : -1;
    MasteryCount[CardId] = std::max(MasteryCount[CardId], 0);

    if (CardGroup)
    {
        LastAnsweredGroup = CardGroup(CardId);
    }
}

bool FMasteryLoopStage::IsMastered(const std::string& CardId) const
{
    const auto Found = MasteryCount.find(CardId);

    return Found != MasteryCount.end() && Found->second >= RepeatsToMaster;
}

bool FMasteryLoopStage::IsBatchCardMastered(int32 BatchIndex) const
{
    if (BatchIndex < 0 || BatchIndex >= static_cast<int32>(BatchCards.size()))
    {
        return false;
    }

    return IsMastered(BatchCards[static_cast<size_t>(BatchIndex)]->Id);
}

std::shared_ptr<FUnmasteredSelector> FMasteryLoopStage::MakeBatchSelector() const
{
    auto Selector = std::make_shared<FUnmasteredSelector>(static_cast<int32>(BatchCards.size()),
        [this](int32 BatchIndex) { return IsBatchCardMastered(BatchIndex); });

    KeepLastGroupOffTheFront(*Selector);

    return Selector;
}

void FMasteryLoopStage::KeepLastGroupOffTheFront(FUnmasteredSelector& Selector) const
{
    if (!CardGroup || LastAnsweredGroup.empty())
    {
        return;
    }

    std::vector<int32> Entries = Selector.GetEntries();

    const auto IsShownEntry = [this](int32 BatchIndex) { return !IsBatchCardMastered(BatchIndex); };
    const auto IsLastGroup = [this](int32 BatchIndex)
    {
        return CardGroup(BatchCards[static_cast<size_t>(BatchIndex)]->Id) == LastAnsweredGroup;
    };

    const auto First = std::find_if(Entries.rbegin(), Entries.rend(), IsShownEntry);

    if (First == Entries.rend() || !IsLastGroup(*First))
    {
        return;
    }

    const auto Other = std::find_if(std::next(First), Entries.rend(),
        [&](int32 BatchIndex) { return IsShownEntry(BatchIndex) && !IsLastGroup(BatchIndex); });

    if (Other == Entries.rend())
    {
        return;
    }

    std::iter_swap(First, Other);

    Selector.SetEntries(std::move(Entries));
}

std::vector<FTCardHandle> FMasteryLoopStage::PickGroupBatch() const
{
    const std::vector<FTCardHandle>& Cards = Params.CardState->GetCards();

    for (const std::vector<int>& Group : BatchGroups)
    {
        std::vector<FTCardHandle> Batch;

        for (const int CardIndex : Group)
        {
            const FTCardHandle& Card = Cards[static_cast<size_t>(CardIndex)];

            if (!IsMastered(Card->Id))
            {
                Batch.push_back(Card);
            }
        }

        if (!Batch.empty())
        {
            return Batch;
        }
    }

    return {};
}

FMasteryStageSessionController& FMasteryLoopStage::SelectController()
{
    // fixed order first
    while (ExerciseNumber < InitialOrder.size())
    {
        const int ControllerIndex = InitialOrder[ExerciseNumber++];
        if (ControllerIndex < Candidates.size() && Candidates[ControllerIndex].CanPickFunction())
        {
            return Candidates[ControllerIndex];
        }
    }

    std::vector<int> Valid;
    for (int Index = 0; Index < static_cast<int>(Candidates.size()); ++Index)
    {
        if (Candidates[Index].CanPickFunction())
        {
            Valid.push_back(Index);
        }
    }

    if (Valid.size() == 1)
    {
        return Candidates[Valid.front()];
    }

    double TotalWeight = 0.0;

    std::vector<int> Indices;
    for (int Index : Valid)
    {
        if (Candidates[Index].Controller != ActiveController)
        {
            TotalWeight += Candidates[Index].Weight;
            Indices.push_back(Index);
        }
    }

    std::uniform_real_distribution<double> Picker(0.0, TotalWeight);
    double Remaining = Picker(Generator);

    for (int Index: Indices)
    {
        Remaining -= Candidates[Index].Weight;

        if (Remaining <= 0.0)
        {
            return Candidates[Index];
        }
    }

    ExerciseNumber++;
    return Candidates[Indices.back()];
}

std::vector<FTCardHandle> FMasteryLoopStage::PickBatch()
{
    if (!BatchGroups.empty())
    {
        return PickGroupBatch();
    }

    std::vector<FTCardHandle> ShortOfTarget;
    std::vector<FTCardHandle> Mastered;

    for (const FTCardHandle& Card : Params.CardState->GetCards())
    {
        if (IsMastered(Card->Id))
        {
            Mastered.push_back(Card);
        }
        else
        {
            ShortOfTarget.push_back(Card);
        }
    }

    std::shuffle(ShortOfTarget.begin(), ShortOfTarget.end(), Generator);
    std::shuffle(Mastered.begin(), Mastered.end(), Generator);

    const auto CountOf = [this](const FTCardHandle& Card)
    {
        const auto Found = MasteryCount.find(Card->Id);

        return Found != MasteryCount.end() ? Found->second : 0;
    };

    std::stable_sort(ShortOfTarget.begin(), ShortOfTarget.end(),
                     [&CountOf](const FTCardHandle& Left, const FTCardHandle& Right) { return CountOf(Left) < CountOf(Right); });

    const size_t Target = std::min(*Params.FixedLength, Params.CardState->GetCount());

    std::vector<FTCardHandle> Batch;
    Batch.reserve(Target);

    for (const std::vector<FTCardHandle>& Source : {ShortOfTarget, Mastered})
    {
        for (const FTCardHandle& Card : Source)
        {
            if (Batch.size() >= Target)
            {
                return Batch;
            }

            Batch.push_back(Card);
        }
    }

    return Batch;
}

void FMasteryLoopStage::StartNextBatch()
{
    BatchCards = PickBatch();

    FMasteryStageSessionController& Candidate = SelectController();

    ActiveController = Candidate.Controller;

    FTSessionStartParams ExerciseParams = Params;
    ExerciseParams.CardState = std::make_shared<FCardState>(BatchCards);
    ExerciseParams.Selector = MakeBatchSelector();

    ActiveController->Start(ExerciseParams);

}

namespace
{
    struct FWordLoopParts
    {
        std::vector<FMasteryStageSessionController> Candidates;
        std::vector<int> Order;
        FTSessionStartParams LearnParams;
        std::shared_ptr<const FGapSentences> GapSentences;
    };

    FWordLoopParts BuildWordLoopParts(QObject* Parent, const FTSessionStartParams& Params, SessionSettings& Options,
                                      std::optional<FGapSentences> SavedGapSentences)
    {
        FWordLoopParts Parts;
        Parts.LearnParams = Params;
        Parts.LearnParams.FixedLength = FTSettings::Get().GetIntensiveLearningRules().CardsPerExercise;

        auto* AudioInstance = new AudioTranscriptionController(Parent);
        AudioInstance->SetSessionSettings(&Options);
        Parts.Candidates.push_back(FMasteryStageSessionController{AudioInstance, 35.0, [&Options]
        {
            return Options.GetRules().bAudioExercise;
        }});

        auto* TypeWordInstance = new WrittenProductionController(Parent);
        TypeWordInstance->SetAskWord(false);
        TypeWordInstance->SetSessionSettings(&Options);
        Parts.Candidates.push_back(FMasteryStageSessionController{TypeWordInstance, 30.0, [&Options]
        {
            return Options.GetRules().bTypeWordExercise;
        }});

        if (!SavedGapSentences.has_value() && Options.GetRules().bGapExercise && Params.CardState)
        {
            SavedGapSentences = PickGapSentences(*Params.CardState);
        }

        Parts.GapSentences = std::make_shared<const FGapSentences>(std::move(SavedGapSentences).value_or(FGapSentences{}));

        auto* GapInstance = new WrittenProductionController(Parent);
        GapInstance->SetAskWord(false);
        GapInstance->SetSessionSettings(&Options);
        GapInstance->SetContentBuilder(
            [GapSentences = Parts.GapSentences, NativeLanguage = Params.NativeLanguage](const FTCard& Card)
            {
                const auto Found = GapSentences->find(Card.Id);

                return Found != GapSentences->end() ? MakeSentenceGapContent(Card, Found->second, NativeLanguage)
                                                    : FWrittenContent{};
            },
            QCoreApplication::translate("FMasteryLoopStage", "Type the missing word..."));
        Parts.Candidates.push_back(FMasteryStageSessionController{GapInstance, 35.0, [&Options]
        {
            return Options.GetRules().bGapExercise;
        }});

        Parts.Order = Options.IsReview() ? std::vector<int>{0, 1, 2} : std::vector<int>{0};

        return Parts;
    }

    std::unique_ptr<FMasteryLoopStage> MakeSentencePhase(const FTSessionStartParams& Params,
                                                         const std::vector<FSessionSentence>& Sentences,
                                                         SentenceTilesController* Tiles)
    {
        std::vector<FTCardHandle> Cards;
        Cards.reserve(Sentences.size());

        for (const FSessionSentence& Picked : Sentences)
        {
            Cards.push_back(Picked.Card);
        }

        FTSessionStartParams PhaseParams = Params;
        PhaseParams.CardState = std::make_shared<FCardState>(std::move(Cards));
        PhaseParams.FixedLength = PhaseParams.CardState->GetCount();
        PhaseParams.Selector = nullptr;

        std::vector<FMasteryStageSessionController> Candidates;
        Candidates.push_back(FMasteryStageSessionController{Tiles, 1.0, [] { return true; }});

        return std::make_unique<FMasteryLoopStage>(std::move(Candidates), std::vector<int>{0}, std::move(PhaseParams), 1);
    }
}

std::unique_ptr<FMasteryStage> FMasteryStage::Create(
    QObject* Parent, const FTSessionStartParams& Params, SessionSettings& Options,
    std::optional<FGapSentences> SavedGapSentences, std::vector<FSessionSentence> SavedSentences)
{
    FWordLoopParts Parts = BuildWordLoopParts(Parent, Params, Options, std::move(SavedGapSentences));

    auto Stage = std::make_unique<FMasteryStage>(std::move(Parts.Candidates), std::move(Parts.Order),
                                                 std::move(Parts.LearnParams), Options.GetRules().RepeatsToMaster);
    Stage->SetGapSentences(std::move(Parts.GapSentences));

    FSessionSentenceStage Built = BuildSessionSentenceStage(Parent, Params, Options, std::move(SavedSentences));

    if (Built.Sentences.empty())
    {
        return Stage;
    }

    std::vector<std::unique_ptr<FMasteryLoopStage>> Phases;

    for (SentenceTilesController* Tiles : Built.Controllers)
    {
        Phases.push_back(MakeSentencePhase(Params, Built.Sentences, Tiles));
    }

    Stage->AddSentencePhases(std::move(Built.Sentences), std::move(Phases));

    return Stage;
}

FMasteryStage::FPolicy::FPolicy(FMasteryStage& InStage)
    : Stage(InStage)
{
}

int FMasteryStage::FPolicy::GetCurrent()
{
    int Sum = 0;

    for (const FTCardHandle& Card : Stage.GetCards())
    {
        Sum += Stage.GetWordCount(Card->Id);
    }

    return Sum;
}

int FMasteryStage::FPolicy::GetMax()
{
    int Sum = 0;

    for (const FTCardHandle& Card : Stage.GetCards())
    {
        Sum += Stage.GetWordTarget(Card->Id);
    }

    return Sum;
}

float FMasteryStage::FPolicy::GetProgress()
{
    const int Target = GetMax();

    return Target > 0 ? static_cast<float>(GetCurrent()) / static_cast<float>(Target) : 0.0f;
}

int FMasteryStage::FPolicy::GetMasteryTarget()
{
    return Stage.GetRepeatsToMaster() + static_cast<int>(Stage.SentencePhases.size());
}

int FMasteryStage::FPolicy::GetCardMasteryTarget(const std::string& CardId)
{
    return Stage.GetWordTarget(Stage.ResolveWord(CardId));
}

int FMasteryStage::FPolicy::GetCardMasteryCount(const std::string& CardId)
{
    return Stage.GetWordCount(Stage.ResolveWord(CardId));
}

bool FMasteryStage::FPolicy::IsFinished() const
{
    return Stage.IsPhaseFinished(Stage.PhaseIndex);
}

void FMasteryStage::FPolicy::OnAnswerRecorded(const std::string& CardId, bool bCorrect)
{
    if (Stage.PhaseIndex == 0)
    {
        Stage.FMasteryLoopStage::OnAnswerRecorded(CardId, bCorrect);

        return;
    }

    Stage.SentencePhases[static_cast<size_t>(Stage.PhaseIndex - 1)]->OnAnswerRecorded(CardId, bCorrect);
}

FMasteryStage::FMasteryStage(std::vector<FMasteryStageSessionController> InCandidates, std::vector<int> InOrder,
                             FTSessionStartParams InParams, int InRepeatsToMaster)
    : FMasteryLoopStage(std::move(InCandidates), std::move(InOrder), std::move(InParams), InRepeatsToMaster)
    , Policy(*this)
{
    AttachPolicy();
}

void FMasteryStage::AddSentencePhases(std::vector<FSessionSentence> InSentences,
                                      std::vector<std::unique_ptr<FMasteryLoopStage>> InPhases)
{
    Sentences = std::move(InSentences);
    SentencePhases = std::move(InPhases);

    for (const FSessionSentence& Picked : Sentences)
    {
        WordBySentenceCard[Picked.Card->Id] = Picked.WordCardId;
        SentenceCardByWord[Picked.WordCardId] = Picked.Card->Id;
    }

    AttachPolicy();
}

void FMasteryStage::AttachPolicy()
{
    for (ExerciseControllerBase* Controller : GetControllers())
    {
        Controller->SetProgressPolicy(&Policy);
    }
}

int FMasteryStage::GetPhase() const
{
    return PhaseIndex;
}

int FMasteryStage::GetPhaseCount() const
{
    return GetLastPhase() + 1;
}

int FMasteryStage::GetLastPhase() const
{
    return static_cast<int>(SentencePhases.size());
}

FMasteryLoopStage& FMasteryStage::GetPhaseLoop(int Phase)
{
    return Phase == 0 ? *this : *SentencePhases[static_cast<size_t>(Phase - 1)];
}

const std::vector<FSessionSentence>& FMasteryStage::GetSentences() const
{
    return Sentences;
}

bool FMasteryStage::IsPhaseFinished(int Phase) const
{
    return Phase == 0 ? IsLoopFinished() : SentencePhases[static_cast<size_t>(Phase - 1)]->IsFinished();
}

void FMasteryStage::StartPhase(int Phase)
{
    PhaseIndex = Phase;

    qCInfo(LogSession) << "Mastery stage moved to phase" << Phase << "of" << GetLastPhase();

    if (Phase == 0)
    {
        FMasteryLoopStage::Start();

        return;
    }

    SentencePhases[static_cast<size_t>(Phase - 1)]->Start();
}

void FMasteryStage::RestorePhase(int Phase)
{
    PhaseIndex = std::clamp(Phase, 0, GetLastPhase());

    if (PhaseIndex == 0)
    {
        return;
    }

    FMasteryLoopStage::ForceComplete();

    for (int Earlier = 1; Earlier < PhaseIndex; ++Earlier)
    {
        SentencePhases[static_cast<size_t>(Earlier - 1)]->ForceComplete();
    }
}

std::string FMasteryStage::ResolveWord(const std::string& CardId) const
{
    const auto Found = WordBySentenceCard.find(CardId);

    return Found != WordBySentenceCard.end() ? Found->second : CardId;
}

int FMasteryStage::GetWordTarget(const std::string& WordId) const
{
    const int Extra = SentenceCardByWord.contains(WordId) ? static_cast<int>(SentencePhases.size()) : 0;

    return GetRepeatsToMaster() + Extra;
}

int FMasteryStage::GetWordCount(const std::string& WordId)
{
    int Count = FMasteryLoopStage::GetCardMasteryCount(WordId);

    const auto Found = SentenceCardByWord.find(WordId);

    if (Found != SentenceCardByWord.end())
    {
        for (const std::unique_ptr<FMasteryLoopStage>& Phase : SentencePhases)
        {
            Count += Phase->GetCardMasteryCount(Found->second);
        }
    }

    return std::min(Count, GetWordTarget(WordId));
}

ExerciseControllerBase* FMasteryStage::GetActiveController() const
{
    if (PhaseIndex == 0)
    {
        return FMasteryLoopStage::GetActiveController();
    }

    return SentencePhases[static_cast<size_t>(PhaseIndex - 1)]->GetActiveController();
}

std::vector<ExerciseControllerBase*> FMasteryStage::GetControllers() const
{
    std::vector<ExerciseControllerBase*> Result = FMasteryLoopStage::GetControllers();

    for (const std::unique_ptr<FMasteryLoopStage>& Phase : SentencePhases)
    {
        const std::vector<ExerciseControllerBase*> PhaseControllers = Phase->GetControllers();
        Result.insert(Result.end(), PhaseControllers.begin(), PhaseControllers.end());
    }

    return Result;
}

void FMasteryStage::Start()
{
    StartPhase(0);
}

void FMasteryStage::OnControllerStateChanged()
{
    if (PhaseIndex == 0)
    {
        FMasteryLoopStage::OnControllerStateChanged();
    }
    else
    {
        SentencePhases[static_cast<size_t>(PhaseIndex - 1)]->OnControllerStateChanged();
    }

    if (PhaseIndex < GetLastPhase() && IsPhaseFinished(PhaseIndex))
    {
        StartPhase(PhaseIndex + 1);
    }
}

bool FMasteryStage::IsFinished() const
{
    return PhaseIndex == GetLastPhase() && IsPhaseFinished(PhaseIndex);
}

void FMasteryStage::ForceComplete()
{
    FMasteryLoopStage::ForceComplete();

    for (const std::unique_ptr<FMasteryLoopStage>& Phase : SentencePhases)
    {
        Phase->ForceComplete();
    }

    PhaseIndex = GetLastPhase();
}

void FMasteryStage::ForceRestart()
{
    if (PhaseIndex == 0)
    {
        FMasteryLoopStage::ForceRestart();

        return;
    }

    SentencePhases[static_cast<size_t>(PhaseIndex - 1)]->ForceRestart();
}

bool FMasteryStage::SkipPhase()
{
    if (PhaseIndex >= GetLastPhase())
    {
        return false;
    }

    if (PhaseIndex == 0)
    {
        FMasteryLoopStage::ForceComplete();
    }
    else
    {
        SentencePhases[static_cast<size_t>(PhaseIndex - 1)]->ForceComplete();
    }

    StartPhase(PhaseIndex + 1);

    return true;
}
