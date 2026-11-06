#pragma once

#include "ExerciseControllerBase.h"
#include "IExerciseFlowStage.h"
#include "SessionSentenceStage.h"

#include "core/FTSettings.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class FMasteryLoopStage;
class FMasteryStage;
class SessionSettings;

struct FSavedStagePosition
{
    int StageIndex = 0;
    bool bRestoreExercise = true;
};

struct FOptionalStages
{
    const char* Key = nullptr;
    bool FTSessionExerciseRules::* Setting = nullptr;
    bool bEnabled = false;
    int Slot = 0;
    int Count = 0;
    int LegacyCount = 0;
    int ResumeIndex = -1;
    int SavedCountOverride = -1;
};

class StageSessionControllerBase : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool hasActiveSession READ HasActiveSession NOTIFY stateChanged)
    Q_PROPERTY(bool finished READ IsFinished NOTIFY stateChanged)
    Q_PROPERTY(int stage READ GetStage NOTIFY stateChanged)
    Q_PROPERTY(int stageCount READ GetStageCount NOTIFY stateChanged)

    Q_PROPERTY(ExerciseControllerBase* activeController READ GetActiveController NOTIFY stateChanged)

public:
    explicit StageSessionControllerBase(QObject* Parent = nullptr);

    Q_INVOKABLE void completeStage();
    Q_INVOKABLE void restartStage();

    bool HasActiveSession() const;
    bool IsFinished() const;
    int GetStage() const;
    int GetStageCount() const;

    ExerciseControllerBase* GetActiveController() const;

signals:
    void stateChanged();

protected:
    static constexpr int MinimumCardCount = 10;

    virtual bool HasCards() const = 0;
    virtual void BuildStages() = 0;
    virtual void ApplyOutcomes() = 0;
    virtual void DiscardSession() = 0;

    virtual void ForceCompleteCurrentStage();
    virtual void ForceRestartCurrentStage();
    virtual void OnStageAdvance(int CompletingStageIndex);

    void StartStages();
    void ResetStages();
    void AdvanceToNextStage();

    IExerciseFlowStage* GetCurrentStage() const;

    FSavedStagePosition ResolveSavedStage(const QJsonObject& Root) const;

    void ResetOptionalStages();
    void AppendChoiceStage(const FTSessionStartParams& Params, SessionSettings& Options,
                           std::vector<ExerciseControllerBase*>& OwnedControllers);
    void AppendMasteryStage(const FTSessionStartParams& Params, SessionSettings& Options,
                            std::vector<ExerciseControllerBase*>& OwnedControllers,
                            std::optional<FGapSentences> GapSentences);
    void AppendMatchStage(const FTSessionStartParams& Params, SessionSettings& Options,
                          std::vector<ExerciseControllerBase*>& OwnedControllers);
    void SaveOptionalStages(QJsonObject& Json) const;
    bool HaveOptionalStagesChanged(const SessionSettings& Options) const;
    bool RestoreSentencePhase(const QJsonObject& Root);

    static QJsonArray CardIdsToJson(const std::vector<FTCardHandle>& Cards);
    static std::optional<std::vector<FTCardHandle>> CardIdsFromJson(
        const QJsonArray& Ids, const std::function<FTCardHandle(const std::string&)>& ResolveCard);
    static QJsonObject IntCountsToJson(const std::unordered_map<std::string, int>& Counts);
    static std::unordered_map<std::string, int> IntCountsFromJson(const QJsonObject& Counts);
    static QJsonObject MasteryStageToJson(const FMasteryLoopStage& Stage);
    static bool RestoreMasteryStage(FMasteryLoopStage& Stage, const QJsonObject& Saved, const QJsonObject& Root,
                                    const std::function<FTCardHandle(const std::string&)>& ResolveCard);

    std::vector<std::unique_ptr<IExerciseFlowStage>> Stages;
    FMasteryStage* MasteryStage = nullptr;
    int MasteryStageIndex = -1;
    std::vector<FSessionSentence> SavedSessionSentences;
    std::optional<FGapSentences> SavedGapSentences;
    int CurrentStageIndex = 0;
    bool bFinished = false;
    bool bOutcomesApplied = false;

protected slots:
    void OnActiveControllerStateChanged();

private:
    void AppendRoundsStage(ExerciseControllerBase* Controller, int RoundsPerCard, const FTSessionStartParams& Params,
                           std::vector<ExerciseControllerBase*>& OwnedControllers);
    void RecordOptionalStages(const char* Key, bool FTSessionExerciseRules::* Setting, const SessionSettings& Options,
                              int Count, int LegacyCount);
    static int ReadSavedStageCount(const QJsonObject& Root, const FOptionalStages& Optional);
    std::vector<FOptionalStages> ListSavedStageLayout(const QJsonObject& Root) const;

    std::vector<FOptionalStages> OptionalStages;
    bool bBuiltWithSentenceStage = false;
};
