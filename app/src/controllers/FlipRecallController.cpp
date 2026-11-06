#include "FlipRecallController.h"

#include "core/FTLog.h"
#include "core/FTSettings.h"

#include <memory>

FlipRecallController::FlipRecallController(QObject* Parent)
    : ExerciseControllerBase(Parent)
{
}

void FlipRecallController::SetQuestionSideOverride(std::function<bool(const ExerciseControllerBase&)> InOverride)
{
    QuestionSideOverride = std::move(InOverride);
}

bool FlipRecallController::ShouldShowTargetWord() const
{
    return QuestionSideOverride ? QuestionSideOverride(*this) : RollShowsTargetWord();
}

void FlipRecallController::InitializeSession()
{
    ExerciseControllerBase::InitializeSession();

    bShowWord = ShouldShowTargetWord();

    Session->SetResult(true);

    bAnswered = false;
    LastRevealedAnswer.clear();

    emit stateChanged();
}

void FlipRecallController::advance()
{
    if (!Session.has_value())
    {
        return;
    }

    if (bAnswered)
    {
        Answer(true);

        return;
    }

    bAnswered = true;

    LastRevealedAnswer = GetAnswer();

    SpeakTargetWord();

    emit stateChanged();
}

void FlipRecallController::answerYes()
{
    Answer(true);
}

void FlipRecallController::answerNo()
{
    Answer(false);
}

void FlipRecallController::Answer(bool bKnew)
{
    if (!Session.has_value() || !bAnswered || IsFinished())
    {
        return;
    }

    const QString AnsweredWord = ShowsTargetLanguage() ? GetPrompt() : GetAnswer();

    if (bKnew)
    {
        RecordAnswer(true);
    }
    else
    {
        RecordSkip();
    }

    qCDebug(LogSession) << metaObject()->className() << "answer:" << (bKnew ? "knew it" : "did not know")
                        << "for" << AnsweredWord;

    Session->Advance();
    bShowWord = ShouldShowTargetWord();
    bAnswered = false;

    LogIfSessionFinished();

    emit stateChanged();
}

QString FlipRecallController::GetPrompt() const
{
    if (!Session.has_value())
    {
        return {};
    }

    return bShowWord ? QString::fromStdString(Session->GetPrompt()) : QString::fromStdString(Session->GetAnswer());
}

QString FlipRecallController::GetAnswer() const
{
    if (!Session.has_value())
    {
        return {};
    }

    return bShowWord ? QString::fromStdString(Session->GetAnswer()) : QString::fromStdString(Session->GetPrompt());
}

bool FlipRecallController::ShowsTargetLanguage() const
{
    return bShowWord;
}

QJsonObject FlipRecallController::SaveExtraState() const
{
    QJsonObject Json;
    Json["showWord"] = bShowWord;

    return Json;
}

void FlipRecallController::RestoreExtraState(const QJsonObject& Json)
{
    bShowWord = Json["showWord"].toBool(false);
}

void FlipRecallController::SpeakTargetWord() const
{
    SpeakAutomatically(ShowsTargetLanguage() ? GetPrompt() : GetAnswer());
}
