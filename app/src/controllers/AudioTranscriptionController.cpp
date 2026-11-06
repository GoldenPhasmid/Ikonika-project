#include "AudioTranscriptionController.h"

#include "core/FTLog.h"
#include "core/FTSettings.h"

#include <algorithm>
#include <memory>

AudioTranscriptionController::AudioTranscriptionController(QObject* Parent)
    : ExerciseControllerBase(Parent)
{
    bRetypeOnMiss = true;
}

void AudioTranscriptionController::SetAnswerCheck(FAnswerCheck InAnswerCheck)
{
    AnswerCheck = std::move(InAnswerCheck);
}

bool AudioTranscriptionController::IsAcceptedAnswer(const QString& Given) const
{
    if (AnswerCheck)
    {
        return AnswerCheck(Given.toStdString(), LastExpectedAnswer.toStdString());
    }

    const std::string Typed = Given.toStdString();
    const FTAnswerComparisonRules Rules = GetComparisonRules();

    if (IsCorrectGrade(CompareAgainstAny(Typed, LastExpectedAnswer.toStdString(), Rules, GetLastWordWithoutArticle())))
    {
        return true;
    }

    const FTCard* Card = GetCurrentCard();

    return Card != nullptr
        && std::any_of(Card->AlternativeSpellings.begin(), Card->AlternativeSpellings.end(),
            [&](const std::string& Spelling)
            {
                return IsCorrectGrade(CompareAgainstAny(Typed, Spelling, Rules));
            });
}

void AudioTranscriptionController::InitializeSession()
{
    ExerciseControllerBase::InitializeSession();
    
    LastCardTranslation.clear();
    SpeakCurrentWord();
}

void AudioTranscriptionController::startSession()
{
    restart();
}

void AudioTranscriptionController::SpeakCurrentWord()
{
    Speak(GetPrompt());
}

void AudioTranscriptionController::replay()
{
    SpeakCurrentWord();
}

void AudioTranscriptionController::submitAnswer(const QString& Given)
{
    if (Given.trimmed().isEmpty() || SubmitRetype(Given))
    {
        return;
    }

    if (!Session.has_value() || bAnswered || Session->IsFinished())
    {
        return;
    }

    LastCardIndex = Session->GetCurrentCardIndex();
    LastGivenAnswer = Given;
    LastExpectedAnswer = GetPrompt();

    RecordAnswer(IsAcceptedAnswer(Given));

    LastCardTranslation = GetAnswer();

    qCDebug(LogSession) << metaObject()->className() << "answer:" << (WasLastAnswerCorrect() ? "correct" : "wrong")
                        << "- given" << Given << ", expected" << LastExpectedAnswer;

    emit stateChanged();
}

void AudioTranscriptionController::skip()
{
    if (!Session.has_value() || bAnswered || Session->IsFinished())
    {
        return;
    }

    LastCardIndex = Session->GetCurrentCardIndex();
    LastGivenAnswer.clear();
    LastExpectedAnswer = GetPrompt();

    RecordSkip();

    LastCardTranslation = GetAnswer();

    qCDebug(LogSession) << metaObject()->className() << "answer: skipped (don't remember)";

    emit stateChanged();
}

void AudioTranscriptionController::advance()
{
    if (!Session.has_value() || !bAnswered || bRetypePending)
    {
        return;
    }

    Session->Advance();
    bAnswered = false;

    LogIfSessionFinished();

    SpeakCurrentWord();

    emit stateChanged();
}

QJsonObject AudioTranscriptionController::SaveExtraState() const
{
    QJsonObject Json;
    Json["lastExpectedAnswer"] = LastExpectedAnswer;
    Json["lastCardTranslation"] = LastCardTranslation;

    return Json;
}

void AudioTranscriptionController::RestoreExtraState(const QJsonObject& Json)
{
    LastExpectedAnswer = Json["lastExpectedAnswer"].toString();
    LastCardTranslation = Json["lastCardTranslation"].toString();
}

QString AudioTranscriptionController::GetLastExpectedAnswer() const
{
    return LastExpectedAnswer;
}

QString AudioTranscriptionController::GetCardTranslation() const
{
    return Session.has_value() ? GetAnswer() : QString();
}

QString AudioTranscriptionController::GetLastCardTranslation() const
{
    return LastCardTranslation;
}
