#pragma once

#include "core/FTCardStore.h"

#include "StageSessionControllerBase.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class FMasteryLoopStage;

class RepetitionController : public StageSessionControllerBase
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(int cardCount READ GetCardCount NOTIFY stateChanged)
    Q_PROPERTY(int elapsedSeconds READ GetElapsedSeconds NOTIFY stateChanged)

public:
    explicit RepetitionController(QObject* Parent = nullptr);

    Q_INVOKABLE bool startDueToday();
    Q_INVOKABLE void restartSession();
    Q_INVOKABLE void endSession();
    Q_INVOKABLE bool removeCard(const QString& cardId);

    int GetCardCount() const;
    int GetElapsedSeconds() const;
    bool IsQuickPass() const;

signals:
    void sessionEnded();

protected:
    bool HasCards() const override;
    void BuildStages() override;
    void ApplyOutcomes() override;
    void DiscardSession() override;
    void ForceCompleteCurrentStage() override;

private:
    void BeginNewSession(std::vector<FTCardHandle> Cards, bool bInQuickPass,
                         std::unordered_set<std::string> InCarriedCardIds);
    void StartAfterQuickPass(const std::unordered_set<std::string>& Forgotten);
    void SkipEmptyReviewStage();
    bool IsCarried(const std::string& CardId) const;
    void DestroyControllers();
    void ConnectStageControllers();
    void OnCorrectIncorrectAnswered(const std::string& CardId, bool bPositive);

    void SaveCheckpoint() const;
    bool LoadCheckpoint(const std::string& Json);
    void TryResumeCheckpoint();
    QByteArray BuildCheckpointJson() const;

    std::shared_ptr<std::vector<FTCardHandle>> Batch = std::make_shared<std::vector<FTCardHandle>>();
    FMasteryLoopStage* MixedStage = nullptr;
    std::vector<ExerciseControllerBase*> OwnedControllers;

    std::shared_ptr<FCardState> CardState;
    std::unordered_map<std::string, bool> ReviewedYes;
    std::unordered_set<std::string> CarriedCardIds;
    bool bQuickPass = false;
    int ReviewCardCount = 0;

    QElapsedTimer SessionTimer;
    qint64 AccumulatedElapsedMs = 0;

    bool bResumingCheckpoint = false;
};
