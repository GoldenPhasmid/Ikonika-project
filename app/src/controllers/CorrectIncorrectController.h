#pragma once

#include "core/FTCardStore.h"

#include "ExerciseControllerBase.h"

#include "core/sessions/FTExerciseSession.h"

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include <string>
#include <vector>

class CorrectIncorrectController : public ExerciseControllerBase
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool lastAnswerCorrect READ WasLastAnswerCorrect NOTIFY stateChanged)
public:
    explicit CorrectIncorrectController(QObject* Parent = nullptr);

    Q_INVOKABLE void reveal();
    Q_INVOKABLE void answerYes();
    Q_INVOKABLE void answerNo();

protected:
    void InitializeSession() override;

signals:
    void answerRecorded(const std::string& CardId, bool bPositive);

private:
    void SpeakPrompt();
    void Answer(bool bPositive);
};
