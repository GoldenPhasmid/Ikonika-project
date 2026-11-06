#include "CorrectIncorrectController.h"

#include "core/FTLog.h"

#include <memory>

CorrectIncorrectController::CorrectIncorrectController(QObject* Parent)
    : ExerciseControllerBase(Parent)
{
}

void CorrectIncorrectController::InitializeSession()
{
    ExerciseControllerBase::InitializeSession();
    
    SpeakPrompt();
}

void CorrectIncorrectController::reveal()
{
    if (!Session.has_value())
    {
        return;
    }

    bAnswered = true;
    LastRevealedAnswer = GetAnswer();

    emit stateChanged();
}

void CorrectIncorrectController::answerYes()
{
    if (!Session.has_value() || !bAnswered)
    {
        return;
    }

    Answer(true);
}

void CorrectIncorrectController::answerNo()
{
    if (!Session.has_value() || !bAnswered)
    {
        return;
    }

    Answer(false);
}

void CorrectIncorrectController::Answer(bool bPositive)
{
    const QString AnsweredWord = GetPrompt();
    const FTCard* Current = GetCurrentCard();

    RecordAnswer(bPositive);

    qCDebug(LogSession) << metaObject()->className() << "answer:" << (bPositive ? "yes" : "no")
                        << "for" << AnsweredWord;

    if (Current != nullptr)
    {
        emit answerRecorded(Current->Id, bPositive);
    }

    Session->Advance();
    bAnswered = false;

    LogIfSessionFinished();

    SpeakPrompt();

    emit stateChanged();
}

void CorrectIncorrectController::SpeakPrompt()
{
    SpeakAutomatically(GetPrompt());
}
