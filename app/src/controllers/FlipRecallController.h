#pragma once

#include "core/FTCardStore.h"

#include "ExerciseControllerBase.h"

#include "core/sessions/FTExerciseSession.h"

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include <functional>

class FlipRecallController : public ExerciseControllerBase
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit FlipRecallController(QObject* Parent = nullptr);

    Q_INVOKABLE void advance() override;
    Q_INVOKABLE void answerYes();
    Q_INVOKABLE void answerNo();

    QString GetPrompt() const override;
    QString GetAnswer() const override;
    bool ShowsTargetLanguage() const override;

    void SetQuestionSideOverride(std::function<bool(const ExerciseControllerBase&)> InOverride);

private:
    void InitializeSession() override;
    void SpeakTargetWord() const;
    void Answer(bool bKnew);
    bool ShouldShowTargetWord() const;

    QJsonObject SaveExtraState() const override;
    void RestoreExtraState(const QJsonObject& Json) override;

    bool bShowWord = false;

    std::function<bool(const ExerciseControllerBase&)> QuestionSideOverride;
};
