#pragma once

#include "core/FTSettings.h"

#include <QObject>
#include <QtQml/qqmlregistration.h>

#include <functional>
#include <string>

class SessionSettings : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by C++, reached through SettingsStore or an exercise controller")

    Q_PROPERTY(bool review READ IsReview CONSTANT)

    Q_PROPERTY(int flipCount READ GetFlipCount WRITE SetFlipCount NOTIFY changed)
    Q_PROPERTY(int maximumFlipCount READ GetMaximumFlipCount CONSTANT)

    Q_PROPERTY(bool choiceStage READ IsChoiceStage WRITE SetChoiceStage NOTIFY changed)
    Q_PROPERTY(int choiceCount READ GetChoiceCount WRITE SetChoiceCount NOTIFY changed)
    Q_PROPERTY(int choiceSide READ GetChoiceSide WRITE SetChoiceSide NOTIFY changed)
    Q_PROPERTY(bool choiceVoice READ IsChoiceVoice WRITE SetChoiceVoice NOTIFY changed)

    Q_PROPERTY(int repeatsToMaster READ GetRepeatsToMaster WRITE SetRepeatsToMaster NOTIFY changed)
    Q_PROPERTY(bool typeWord READ IsTypeWord WRITE SetTypeWord NOTIFY changed)
    Q_PROPERTY(bool gapExercise READ IsGapExercise WRITE SetGapExercise NOTIFY changed)
    Q_PROPERTY(bool audioExercise READ IsAudioExercise WRITE SetAudioExercise NOTIFY changed)
    Q_PROPERTY(bool requireExactAccents READ AreExactAccentsRequired WRITE SetRequireExactAccents NOTIFY changed)

    Q_PROPERTY(bool sentenceStage READ IsSentenceStage WRITE SetSentenceStage NOTIFY changed)
    Q_PROPERTY(bool sentenceTyping READ IsSentenceTyping WRITE SetSentenceTyping NOTIFY changed)

    Q_PROPERTY(bool matchStage READ IsMatchStage WRITE SetMatchStage NOTIFY changed)
    Q_PROPERTY(int matchCount READ GetMatchCount WRITE SetMatchCount NOTIFY changed)
    Q_PROPERTY(bool matchVoice READ IsMatchVoice WRITE SetMatchVoice NOTIFY changed)

    Q_PROPERTY(int maximumStageRepeatCount READ GetMaximumStageRepeatCount CONSTANT)

public:
    static SessionSettings& Training();
    static SessionSettings& Review();

    const FTSessionExerciseRules& GetRules() const;
    const std::string& GetGroup() const;

    bool IsReview() const;

    int GetFlipCount() const;
    void SetFlipCount(int Value);
    int GetMaximumFlipCount() const;

    bool IsChoiceStage() const;
    void SetChoiceStage(bool bValue);
    int GetChoiceCount() const;
    void SetChoiceCount(int Value);
    int GetChoiceSide() const;
    void SetChoiceSide(int Value);
    bool IsChoiceVoice() const;
    void SetChoiceVoice(bool bValue);

    int GetRepeatsToMaster() const;
    void SetRepeatsToMaster(int Value);
    bool IsTypeWord() const;
    void SetTypeWord(bool bValue);
    bool IsGapExercise() const;
    void SetGapExercise(bool bValue);
    bool IsAudioExercise() const;
    void SetAudioExercise(bool bValue);
    bool AreExactAccentsRequired() const;
    void SetRequireExactAccents(bool bValue);

    bool IsSentenceStage() const;
    void SetSentenceStage(bool bValue);
    bool IsSentenceTyping() const;
    void SetSentenceTyping(bool bValue);

    bool IsMatchStage() const;
    void SetMatchStage(bool bValue);
    int GetMatchCount() const;
    void SetMatchCount(int Value);
    bool IsMatchVoice() const;
    void SetMatchVoice(bool bValue);

    int GetMaximumStageRepeatCount() const;

    static void NotifyDisplayChanged();

signals:
    void changed();

private:
    explicit SessionSettings(std::string InGroup);

    void Update(const char* Name, const QVariant& Value, const std::function<void(FTSessionExerciseRules&)>& Change);

    std::string Group;
};
