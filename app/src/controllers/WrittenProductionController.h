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
    std::string PromptSpeech;
    std::string Translation;

    std::string RevealedAnswer;
    std::string RevealedSpeech;

    bool bSentence = false;
};

using FWrittenContentBuilder = std::function<FWrittenContent(const FTCard& Card)>;

class WrittenProductionController : public ExerciseControllerBase
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString directionLabel READ GetDirectionLabel NOTIFY stateChanged)
    Q_PROPERTY(QString answerHint READ GetAnswerHint NOTIFY stateChanged)
    Q_PROPERTY(bool hasPromptSpeech READ HasPromptSpeech NOTIFY stateChanged)
    Q_PROPERTY(QString promptHeading READ GetPromptHeading NOTIFY stateChanged)
    Q_PROPERTY(QString lastCardTranslation READ GetLastCardTranslation NOTIFY stateChanged)
    Q_PROPERTY(bool sentencePrompt READ IsSentencePrompt NOTIFY stateChanged)

public:
    explicit WrittenProductionController(QObject* Parent = nullptr);

    void SetAskWord(bool bInAskWord);

    void SetContentBuilder(FWrittenContentBuilder InContentBuilder, const QString& InAnswerHint);

    QString GetAnswerHint() const;
    bool HasPromptSpeech() const;
    QString GetLastCardTranslation() const;
    bool IsSentencePrompt() const;

    Q_INVOKABLE void startSession();
    Q_INVOKABLE void submitAnswer(const QString& Given);
    Q_INVOKABLE void skip();
    Q_INVOKABLE void advance() override;
    Q_INVOKABLE void replay();

    QString GetPrompt() const override;
    QString GetPromptHeading() const;
    QString GetAnswer() const override;
    bool ShowsTargetLanguage() const override;

    QString GetDirectionLabel() const;

private:
    void InitializeSession() override;

    QJsonObject SaveExtraState() const override;
    void RestoreExtraState(const QJsonObject& Json) override;

    std::optional<FWrittenContent> BuildContent() const;
    std::string GetPromptSpeech() const;
    QString GetCardTranslation() const;
    void SpeakPrompt() const;
    void SpeakNewPrompt() const;
    void SpeakAnswerIfTargetWord();
    bool IsAcceptedAnswer(const QString& Given) const override;

    bool bAskWord = true;

    FWrittenContentBuilder ContentBuilder;
    QString AnswerHint;
    QString LastCardTranslation;
};
