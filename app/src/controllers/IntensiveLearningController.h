#pragma once

#include "core/FTCardStore.h"

#include "StageSessionControllerBase.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QString>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

class FRandomWeightedSelector;
class FMasteryLoopStage;

class IntensiveLearningController : public StageSessionControllerBase
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString title READ GetTitle NOTIFY stateChanged)
    Q_PROPERTY(int elapsedSeconds READ GetElapsedSeconds NOTIFY stateChanged)
    Q_PROPERTY(int sessionWordCount READ GetSessionWordCount NOTIFY stateChanged)
    Q_PROPERTY(bool formsSession READ IsFormsSession NOTIFY stateChanged)

    Q_PROPERTY(bool debugBuild READ IsDebugBuild CONSTANT)

    Q_PROPERTY(QString vocabularyKey READ GetVocabularyKey CONSTANT)

public:
    explicit IntensiveLearningController(QObject* Parent = nullptr);

    Q_INVOKABLE bool startCards(const QString& SessionKey, const QStringList& CardIds);
    Q_INVOKABLE bool resume(const QString& SessionKey);
    Q_INVOKABLE bool hasSessionFor(const QString& SessionKey) const;
    Q_INVOKABLE QString lastActiveKey() const;
    Q_INVOKABLE int stageFor(const QString& SessionKey) const;
    Q_INVOKABLE int wordCountFor(const QString& SessionKey) const;

    Q_INVOKABLE void restartSession();
    Q_INVOKABLE void endSession();
    Q_INVOKABLE bool removeCard(const QString& cardId);

    QString GetTitle() const;
    static QString GetVocabularyKey();
    int GetElapsedSeconds() const;
    int GetSessionWordCount() const;
    bool IsFormsSession() const;

    void RestoreCheckpoints(std::vector<std::pair<std::string, std::string>> Stored);

    static constexpr bool IsDebugBuild()
    {
#ifdef QT_DEBUG
        return true;
#else
        return false;
#endif
    }

signals:
    void sessionEnded(const QString& sessionKey);

protected:
    bool HasCards() const override;
    void BuildStages() override;
    void ApplyOutcomes() override;
    void DiscardSession() override;

private:
    void BeginNewSession(std::shared_ptr<FCardState> InCardState, const QString& InTitle);
    void DestroyControllers();
    void ConnectStageControllers();

    bool IsActiveFor(const QString& InSessionKey) const;
    bool HasFormsRulesChanged(bool bSavedOptionalPronoun) const;
    void RebuildForChangedSettings();

    void SaveCheckpoint() const;
    bool LoadCheckpoint(const std::string& Json);
    void TryResumeCheckpoint();
    QByteArray BuildCheckpointJson() const;

    QString SessionKey;
    QString Title;
    std::shared_ptr<FCardState> CardState;
    std::vector<ExerciseControllerBase*> OwnedControllers;
    FMasteryLoopStage* MixedStage = nullptr;
    FMasteryLoopStage* FlipStage = nullptr;
    uint32_t FormsSentenceSeed = 0;
    bool bBuiltWithOptionalPronoun = true;

    QElapsedTimer SessionTimer;
    qint64 AccumulatedElapsedMs = 0;

    bool bResumingCheckpoint = false;
};
