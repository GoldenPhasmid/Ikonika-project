#pragma once

#include "StageSessionControllerBase.h"

#include "core/FTSentenceTree.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QString>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class FMasteryLoopStage;

class SentenceTrainingController : public StageSessionControllerBase
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool available READ IsAvailable NOTIFY availabilityChanged)
    Q_PROPERTY(int readyCount READ GetReadyCount NOTIFY availabilityChanged)
    Q_PROPERTY(int sentenceCount READ GetSentenceCount WRITE SetSentenceCount NOTIFY availabilityChanged)
    Q_PROPERTY(int minimumSentenceCount READ GetMinimumSentenceCount CONSTANT)
    Q_PROPERTY(int maximumSentenceCount READ GetMaximumSentenceCount CONSTANT)
    Q_PROPERTY(int sentenceCountStep READ GetSentenceCountStep CONSTANT)
    Q_PROPERTY(bool canStart READ CanStart NOTIFY availabilityChanged)
    Q_PROPERTY(bool unlocked READ IsUnlocked NOTIFY availabilityChanged)
    Q_PROPERTY(int difficultyMask READ GetDifficultyMask WRITE SetDifficultyMask NOTIFY availabilityChanged)
    Q_PROPERTY(int unlockedDifficultyMask READ GetUnlockedDifficultyMask NOTIFY availabilityChanged)

    Q_PROPERTY(int elapsedSeconds READ GetElapsedSeconds NOTIFY stateChanged)
    Q_PROPERTY(int sessionSentenceCount READ GetSessionSentenceCount NOTIFY stateChanged)
    Q_PROPERTY(QStringList savedExercises READ GetSavedExercises NOTIFY stateChanged)

public:
    static constexpr int DefaultSentenceCount = MinimumCardCount;
    static constexpr int MinimumSentenceCount = 5;
    static constexpr int MaximumSentenceCount = 20;
    static constexpr int SentenceCountStep = 5;
    static constexpr int UnlockSentenceCount = 10;
    static constexpr int CandidatesPerSentence = 15;
    static constexpr int EasyDifficulty = 1;
    static constexpr int NormalDifficulty = 2;
    static constexpr int HardDifficulty = 4;
    static constexpr int EveryDifficulty = EasyDifficulty | NormalDifficulty | HardDifficulty;
    static constexpr float SentenceSpeechRateScale = 0.75f;
    static constexpr int MinimumSentenceWords = 3;
    static constexpr int MaximumSentenceCharacters = 80;

    explicit SentenceTrainingController(QObject* Parent = nullptr);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool start(const QString& Exercise);
    Q_INVOKABLE void endSession();
    Q_INVOKABLE void forgetSessions();

    bool IsAvailable() const;
    int GetReadyCount() const;
    bool CanStart() const;
    bool IsUnlocked() const;

    int GetSentenceCount() const;
    void SetSentenceCount(int InSentenceCount);
    int GetMinimumSentenceCount() const;
    int GetMaximumSentenceCount() const;
    int GetSentenceCountStep() const;

    int GetDifficultyMask() const;
    int GetUnlockedDifficultyMask() const;
    void SetDifficultyMask(int InDifficultyMask);

    static uint32_t MakeGrammarLevelMask(int DifficultyMask);

    int GetElapsedSeconds() const;
    int GetSessionSentenceCount() const;
    QStringList GetSavedExercises() const;

signals:
    void availabilityChanged();

protected:
    bool HasCards() const override;
    void BuildStages() override;
    void ApplyOutcomes() override;
    void DiscardSession() override;

private:
    std::vector<FTTreeSentence> FindCandidates(int InDifficultyMask, int InSentenceCount) const;

    void AdoptController(ExerciseControllerBase* Controller);
    void AddFlashcardStage(const FTSessionStartParams& Params);
    void AddStage(ExerciseControllerBase* Controller, const FTSessionStartParams& Params);
    void DestroyControllers();
    void ReleaseSession();
    void ForgetCheckpoint(const QString& InExercise);
    FMasteryLoopStage* GetCurrentMasteryStage() const;

    void SaveCheckpoint() const;
    bool LoadCheckpoint(const std::string& Json);
    void LoadStoredCheckpoints();
    QByteArray BuildCheckpointJson() const;

    QString Exercise;
    std::shared_ptr<FCardState> CardState;
    std::unordered_map<std::string, FTTreeSentence> Sentences;
    std::vector<ExerciseControllerBase*> OwnedControllers;
    std::vector<FMasteryLoopStage*> MasteryStages;

    bool bAvailable = false;
    bool bUnlocked = false;
    int UnlockedDifficultyMask = 0;
    int ReadyCount = 0;
    std::string RefreshedRevision;
    int DifficultyMask = EasyDifficulty;
    int SentenceCount = DefaultSentenceCount;

    QElapsedTimer SessionTimer;
    qint64 AccumulatedElapsedMs = 0;
    qint64 FinishedElapsedMs = 0;

    bool bResumingCheckpoint = false;
};
