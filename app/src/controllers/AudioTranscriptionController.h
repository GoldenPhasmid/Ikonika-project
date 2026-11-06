#pragma once

#include "core/FTCardStore.h"

#include "ExerciseControllerBase.h"

#include "core/FTAnswerComparison.h"
#include "core/sessions/FTExerciseSession.h"

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

using FAnswerCheck = std::function<bool(const std::string& Given, const std::string& Expected)>;

class AudioTranscriptionController : public ExerciseControllerBase
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString lastExpectedAnswer READ GetLastExpectedAnswer NOTIFY stateChanged)

    Q_PROPERTY(QString cardTranslation READ GetCardTranslation NOTIFY stateChanged)
    Q_PROPERTY(QString lastCardTranslation READ GetLastCardTranslation NOTIFY stateChanged)

public:
    explicit AudioTranscriptionController(QObject* Parent = nullptr);

    void SetAnswerCheck(FAnswerCheck InAnswerCheck);

    Q_INVOKABLE void startSession();

    Q_INVOKABLE void submitAnswer(const QString& Given);
    Q_INVOKABLE void skip();
    Q_INVOKABLE void advance();

    Q_INVOKABLE void replay();

    QString GetLastExpectedAnswer() const;
    QString GetCardTranslation() const;
    QString GetLastCardTranslation() const;

private:
    void InitializeSession() override;

    QJsonObject SaveExtraState() const override;
    void RestoreExtraState(const QJsonObject& Json) override;

    void SpeakCurrentWord();
    bool IsAcceptedAnswer(const QString& Given) const override;

    FAnswerCheck AnswerCheck;

    QString LastExpectedAnswer;
    QString LastCardTranslation;
};
