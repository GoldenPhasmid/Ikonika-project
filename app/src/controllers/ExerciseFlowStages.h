#pragma once

#include "ExerciseControllerBase.h"
#include "IExerciseFlowStage.h"
#include "SessionSentenceStage.h"

#include "core/FTCardStore.h"

#include <QString>

#include <functional>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

class FProxyStage : public IExerciseFlowStage
{
public:
    FProxyStage(ExerciseControllerBase* InController, const FTSessionStartParams& InParams);

    ExerciseControllerBase* GetActiveController() const override;
    std::vector<ExerciseControllerBase*> GetControllers() const override;

    void Start() override;

    void OnControllerStateChanged() override;

    bool IsFinished() const override;

    void ForceComplete() override;
    void ForceRestart() override;

    void RestoreActiveExercise(std::optional<FExerciseCheckpoint> Checkpoint) override;

private:
    ExerciseControllerBase* Controller = nullptr;
    FTSessionStartParams Params;
    bool bForceFinished = false;
};

struct FMasteryStageSessionController
{
    ExerciseControllerBase* Controller = nullptr;
    double Weight = 1.0;
    std::function<bool()> CanPickFunction;
};

struct FMasteryLoopProgress
{
    std::unordered_map<std::string, int> MasteryCount;
    std::unordered_map<std::string, int> ConsecutiveWrongCount;
    int BatchCount = 0;
};

struct FMasteryLoopActiveBatch
{
    int CandidateIndex = -1;
    std::vector<FTCardHandle> BatchCards;
    std::optional<FExerciseCheckpoint> Checkpoint;
};

class FUnmasteredSelector : public FEachOnceSelector
{
public:
    FUnmasteredSelector(int32 InCount, std::function<bool(int32)> InIsMastered);

    std::optional<int> Next() override;

private:
    std::function<bool(int32)> IsMastered;
};

using FCardGroup = std::function<std::string(const std::string& CardId)>;

class FMasteryLoopStage : public IExerciseFlowStage, public IExerciseProgressPolicy
{
public:
    FMasteryLoopStage(std::vector<FMasteryStageSessionController> InCandidates, std::vector<int> InOrder, FTSessionStartParams InParams, int InRepeatsToMaster);

    void SetCardGroup(FCardGroup InCardGroup);
    void SetBatchGroups(std::vector<std::vector<int>> InBatchGroups);

    int GetCurrent() override;
    int GetMax() override;
    float GetProgress() override;
    int GetMasteryTarget() override;
    int GetCardMasteryCount(const std::string& CardId) override;
    void OnAnswerRecorded(const std::string& CardId, bool bCorrect) override;

    ExerciseControllerBase* GetActiveController() const override;
    std::vector<ExerciseControllerBase*> GetControllers() const override;

    void Start() override;

    void OnControllerStateChanged() override;

    bool IsFinished() const override;
    bool IsLoopFinished() const;

    void ForceComplete() override;
    void ForceRestart() override;

    FMasteryLoopProgress SaveProgress() const;
    void RestoreProgress(const FMasteryLoopProgress& Progress, const FMasteryLoopActiveBatch& ActiveBatch = {});

    int GetActiveCandidateIndex() const;
    const std::vector<FTCardHandle>& GetBatchCards() const { return BatchCards; }

    int GetActiveCount() const;
    int GetMasteredCount() const;

    const FGapSentences& GetGapSentences() const;
    void SetGapSentences(std::shared_ptr<const FGapSentences> InGapSentences);

    const std::vector<FTCardHandle>& GetCards() const;
    int GetRepeatsToMaster() const { return RepeatsToMaster; }

private:
    int GetEarnedAnswers();
    bool IsMastered(const std::string& CardId) const;
    bool IsBatchCardMastered(int32 BatchIndex) const;
    std::shared_ptr<FUnmasteredSelector> MakeBatchSelector() const;
    void KeepLastGroupOffTheFront(FUnmasteredSelector& Selector) const;
    std::vector<FTCardHandle> PickGroupBatch() const;
    FMasteryStageSessionController& SelectController();
    std::vector<FTCardHandle> PickBatch();
    void StartNextBatch();

    std::vector<FMasteryStageSessionController> Candidates;
    std::vector<int> InitialOrder;
    FTSessionStartParams Params;
    int RepeatsToMaster = 0;
    std::mt19937 Generator{MakeRandomSeed()};

    ExerciseControllerBase* ActiveController = nullptr;
    std::vector<FTCardHandle> BatchCards;
    std::unordered_map<std::string, int> MasteryCount;
    FCardGroup CardGroup;
    std::vector<std::vector<int>> BatchGroups;
    std::string LastAnsweredGroup;
    std::shared_ptr<const FGapSentences> GapSentences = std::make_shared<const FGapSentences>();
    /** number of picked exercise controllers */
    int ExerciseNumber = 0;
    int BatchCount = 0;
};

class FMasteryStage : public FMasteryLoopStage
{
public:
    static std::unique_ptr<FMasteryStage> Create(
        QObject* Parent, const FTSessionStartParams& Params, SessionSettings& Options,
        std::optional<FGapSentences> SavedGapSentences = std::nullopt,
        std::vector<FSessionSentence> SavedSentences = {});

    FMasteryStage(std::vector<FMasteryStageSessionController> InCandidates, std::vector<int> InOrder,
                  FTSessionStartParams InParams, int InRepeatsToMaster);

    void AddSentencePhases(std::vector<FSessionSentence> InSentences,
                           std::vector<std::unique_ptr<FMasteryLoopStage>> InPhases);

    int GetPhase() const;
    int GetPhaseCount() const;
    FMasteryLoopStage& GetPhaseLoop(int Phase);
    const std::vector<FSessionSentence>& GetSentences() const;
    void RestorePhase(int Phase);

    ExerciseControllerBase* GetActiveController() const override;
    std::vector<ExerciseControllerBase*> GetControllers() const override;

    void Start() override;

    void OnControllerStateChanged() override;

    bool IsFinished() const override;

    void ForceComplete() override;
    void ForceRestart() override;
    bool SkipPhase() override;

private:
    class FPolicy : public IExerciseProgressPolicy
    {
    public:
        explicit FPolicy(FMasteryStage& InStage);

        int GetCurrent() override;
        int GetMax() override;
        float GetProgress() override;
        int GetMasteryTarget() override;
        int GetCardMasteryTarget(const std::string& CardId) override;
        int GetCardMasteryCount(const std::string& CardId) override;
        bool IsFinished() const override;
        void OnAnswerRecorded(const std::string& CardId, bool bCorrect) override;

    private:
        FMasteryStage& Stage;
    };

    int GetLastPhase() const;
    bool IsPhaseFinished(int Phase) const;
    void StartPhase(int Phase);
    void AttachPolicy();

    std::string ResolveWord(const std::string& CardId) const;
    int GetWordTarget(const std::string& WordId) const;
    int GetWordCount(const std::string& WordId);

    FPolicy Policy;
    std::vector<std::unique_ptr<FMasteryLoopStage>> SentencePhases;
    std::vector<FSessionSentence> Sentences;
    std::unordered_map<std::string, std::string> WordBySentenceCard;
    std::unordered_map<std::string, std::string> SentenceCardByWord;
    int PhaseIndex = 0;
};
