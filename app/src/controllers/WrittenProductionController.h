#pragma once

#include "core/FTCardStore.h"

#include "ExerciseControllerBase.h"

#include "core/FTAnswerComparison.h"
#include "core/sessions/FTExerciseSession.h"

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

struct FWrittenContent
{
    std::string Heading;
    std::string Prompt;
    std::vector<std::string> AcceptedAnswers;
    std::string Translation;

    std::string RevealedAnswer;

    bool bSentence = false;
};

struct FExerciseSpeech
{
    QString Text;
    bool bSentence = false;
};

class WrittenProductionController : public ExerciseControllerBase
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString directionLabel READ GetDirectionLabel NOTIFY stateChanged)
    Q_PROPERTY(QString answerHint READ GetAnswerHint NOTIFY stateChanged)
    Q_PROPERTY(bool canReplay READ CanReplay NOTIFY stateChanged)
    Q_PROPERTY(QString promptHeading READ GetPromptHeading NOTIFY stateChanged)
    Q_PROPERTY(QString lastCardTranslation READ GetLastCardTranslation NOTIFY stateChanged)
    Q_PROPERTY(bool sentencePrompt READ IsSentencePrompt NOTIFY stateChanged)

public:
    explicit WrittenProductionController(QObject* Parent = nullptr);

    void SetAskWord(bool bInAskWord);

    QString GetAnswerHint() const;
    bool CanReplay() const;
    QString GetLastCardTranslation() const;
    bool IsSentencePrompt() const;

    Q_INVOKABLE void startSession();
    Q_INVOKABLE void submitAnswer(const QString& Given);
    Q_INVOKABLE void skip();
    Q_INVOKABLE void advance() override;
    Q_INVOKABLE void replay();
    void speakSlowly() override;

    QString GetPrompt() const override;
    QString GetPromptHeading() const;
    QString GetAnswer() const override;
    bool ShowsTargetLanguage() const override;

    QString GetDirectionLabel() const;

protected:
    virtual std::optional<FWrittenContent> BuildCardContent(const FTCard& Card) const;
    virtual FExerciseSpeech GetHiddenSpeech() const;
    virtual FExerciseSpeech GetRevealSpeech() const;

    QString AnswerHint;

private:
    void InitializeSession() override;

    QJsonObject SaveExtraState() const override;
    void RestoreExtraState(const QJsonObject& Json) override;

    std::optional<FWrittenContent> BuildContent() const;
    QString GetCardTranslation() const;
    FExerciseSpeech GetStateSpeech() const;
    void SpeakState(float RateScale) const;
    void SpeakStateAutomatically() const;
    bool IsAcceptedAnswer(const QString& Given) const override;

    bool bAskWord = true;

    QString LastCardTranslation;
};
