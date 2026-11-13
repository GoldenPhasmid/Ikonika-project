#include "WrittenProductionController.h"
#include "AppSpeech.h"

#include "core/FTLog.h"
#include "core/FTSettings.h"

#include <memory>

WrittenProductionController::WrittenProductionController(QObject* Parent)
    : ExerciseControllerBase(Parent)
{
    bRetypeOnMiss = true;
}

void WrittenProductionController::SetAskWord(bool bInAskWord)
{
    bAskWord = bInAskWord;
}

QString WrittenProductionController::GetAnswerHint() const
{
    return BuildContent().has_value() ? AnswerHint : QString();
}

QString WrittenProductionController::GetDirectionLabel() const
{
    const QString Target = TargetLanguage.toUpper();
    const QString Native =
        QString::fromStdString(FTSettings::Get().GetLanguageRules().NativeLanguage).toUpper();

    return bAskWord
        ? QStringLiteral("%1 → %2").arg(Target, Native)
        : QStringLiteral("%1 → %2").arg(Native, Target);
}

void WrittenProductionController::startSession()
{
    restart();
}

void WrittenProductionController::InitializeSession()
{
    ExerciseControllerBase::InitializeSession();

    LastCardTranslation.clear();

    qCDebug(LogSession) << metaObject()->className() << "direction:" << GetDirectionLabel();

    SpeakStateAutomatically();
}

std::optional<FWrittenContent> WrittenProductionController::BuildCardContent(const FTCard& Card) const
{
    Q_UNUSED(Card);

    return std::nullopt;
}

std::optional<FWrittenContent> WrittenProductionController::BuildContent() const
{
    const FTCard* Current = Session.has_value() ? Session->GetCurrentCard() : nullptr;

    if (Current == nullptr)
    {
        return std::nullopt;
    }

    std::optional<FWrittenContent> Content = BuildCardContent(*Current);

    if (!Content.has_value() || Content->Prompt.empty())
    {
        return std::nullopt;
    }

    return Content;
}

QJsonObject WrittenProductionController::SaveExtraState() const
{
    QJsonObject Json;
    Json["lastCardTranslation"] = LastCardTranslation;

    return Json;
}

void WrittenProductionController::RestoreExtraState(const QJsonObject& Json)
{
    LastCardTranslation = Json["lastCardTranslation"].toString();
}

QString WrittenProductionController::GetCardTranslation() const
{
    const std::optional<FWrittenContent> Content = BuildContent();

    if (Content.has_value() && !Content->Translation.empty())
    {
        return QString::fromStdString(Content->Translation);
    }

    return QString::fromStdString(Session->GetAnswer());
}

QString WrittenProductionController::GetLastCardTranslation() const
{
    return LastCardTranslation;
}

bool WrittenProductionController::IsSentencePrompt() const
{
    const std::optional<FWrittenContent> Content = BuildContent();

    return Content.has_value() && Content->bSentence;
}

bool WrittenProductionController::CanReplay() const
{
    return !GetStateSpeech().Text.isEmpty();
}

FExerciseSpeech WrittenProductionController::GetHiddenSpeech() const
{
    if (!bAskWord)
    {
        return {};
    }

    return FExerciseSpeech{GetPrompt()};
}

FExerciseSpeech WrittenProductionController::GetRevealSpeech() const
{
    if (bAskWord)
    {
        return {};
    }

    return FExerciseSpeech{LastRevealedAnswer};
}

FExerciseSpeech WrittenProductionController::GetStateSpeech() const
{
    if (!Session.has_value() || Session->IsFinished())
    {
        return {};
    }

    return bAnswered ? GetRevealSpeech() : GetHiddenSpeech();
}

void WrittenProductionController::SpeakState(float RateScale) const
{
    const FExerciseSpeech Speech = GetStateSpeech();

    if (Speech.Text.isEmpty())
    {
        qCInfo(LogSpeech) << metaObject()->className() << "has nothing to say while"
                          << (bAnswered ? "revealed" : "hidden");

        return;
    }

    const float SentenceScale = Speech.bSentence ? AppSpeech::Get().GetSentenceSpeed() : 1.0f;

    SpeakAtScale(Speech.Text, SentenceScale * RateScale);
}

void WrittenProductionController::SpeakStateAutomatically() const
{
    if (GetStateSpeech().Text.isEmpty() || !AppSpeech::Get().AllowsAutomaticSpeech())
    {
        return;
    }

    SpeakState(1.0f);
}

void WrittenProductionController::replay()
{
    SpeakState(1.0f);
}

void WrittenProductionController::speakSlowly()
{
    qCInfo(LogSpeech) << metaObject()->className() << "slow speech requested";

    SpeakState(AppSpeech::SlowRateScale);
}

QString WrittenProductionController::GetPromptHeading() const
{
    const std::optional<FWrittenContent> Content = BuildContent();

    return Content.has_value() ? QString::fromStdString(Content->Heading) : QString();
}

QString WrittenProductionController::GetPrompt() const
{
    if (!Session.has_value())
    {
        return {};
    }

    if (const std::optional<FWrittenContent> Content = BuildContent())
    {
        return QString::fromStdString(Content->Prompt);
    }

    return bAskWord ? QString::fromStdString(Session->GetPrompt()) : QString::fromStdString(Session->GetAnswer());
}

QString WrittenProductionController::GetAnswer() const
{
    if (!Session.has_value())
    {
        return {};
    }

    const std::optional<FWrittenContent> Content = BuildContent();

    if (Content.has_value() && !Content->RevealedAnswer.empty())
    {
        return QString::fromStdString(Content->RevealedAnswer);
    }

    return bAskWord ? QString::fromStdString(Session->GetAnswer()) : QString::fromStdString(Session->GetPrompt());
}

bool WrittenProductionController::ShowsTargetLanguage() const
{
    return bAskWord;
}

void WrittenProductionController::submitAnswer(const QString& Given)
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
    LastRevealedAnswer = GetAnswer();
    LastCardTranslation = GetCardTranslation();

    RecordAnswer(IsAcceptedAnswer(Given));

    qCDebug(LogSession) << metaObject()->className() << "answer:" << (WasLastAnswerCorrect() ? "correct" : "wrong")
                        << "- given" << Given << ", expected" << LastRevealedAnswer;

    SpeakStateAutomatically();

    emit stateChanged();
}

bool WrittenProductionController::IsAcceptedAnswer(const QString& Given) const
{
    const FTAnswerComparisonRules& Rules = GetComparisonRules();
    const std::optional<FWrittenContent> Content = BuildContent();

    if (!Content.has_value())
    {
        const std::string GivenText = Given.toStdString();
        const std::string ExpectedText = LastRevealedAnswer.toStdString();
        const std::string WithoutArticle = bAskWord ? std::string() : GetLastWordWithoutArticle();

        if (IsCorrectGrade(CompareAgainstAny(GivenText, ExpectedText, Rules, WithoutArticle)))
        {
            return true;
        }

        return bAskWord && IsApproximateTranslation(GivenText, ExpectedText, Rules);
    }

    for (const std::string& Accepted : Content->AcceptedAnswers)
    {
        if (IsCorrectGrade(CompareAnswer(Given.toStdString(), Accepted, Rules)))
        {
            return true;
        }
    }

    return false;
}

void WrittenProductionController::skip()
{
    if (!Session.has_value() || bAnswered || Session->IsFinished())
    {
        return;
    }

    LastCardIndex = Session->GetCurrentCardIndex();
    LastGivenAnswer.clear();
    LastRevealedAnswer = GetAnswer();
    LastCardTranslation = GetCardTranslation();

    RecordSkip();

    qCDebug(LogSession) << metaObject()->className() << "answer: skipped (don't remember)";

    SpeakStateAutomatically();

    emit stateChanged();
}

void WrittenProductionController::advance()
{
    if (!Session.has_value() || !bAnswered || bRetypePending)
    {
        return;
    }

    Session->Advance();
    bAnswered = false;

    LogIfSessionFinished();

    SpeakStateAutomatically();

    emit stateChanged();
}
