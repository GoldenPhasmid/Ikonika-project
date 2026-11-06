#include "MultipleChoiceController.h"
#include "AppSpeech.h"
#include "AppCards.h"

#include "core/FTLog.h"
#include "core/FTSettings.h"

#include <QJsonArray>

#include <algorithm>
#include <memory>

namespace
{
    constexpr size_t ChoiceOptionCount = 4;
}

MultipleChoiceController::MultipleChoiceController(QObject* Parent)
    : ExerciseControllerBase(Parent)
{
    MultipleChoiceController::ApplySessionSettings();
}

void MultipleChoiceController::ApplySessionSettings()
{
    SetVoicePrompt(GetSessionRules().bChoiceVoice);
}

void MultipleChoiceController::SetContentBuilder(FChoiceContentBuilder InContentBuilder, const QString& InTitle)
{
    ContentBuilder = std::move(InContentBuilder);
    Title = InTitle;
}

QString MultipleChoiceController::GetTitle() const
{
    return Title;
}

void MultipleChoiceController::InitializeSession()
{
    ExerciseControllerBase::InitializeSession();

    bShowWord = (SupportsVoicePrompt() && IsVoicePromptRequested()) || RollShowsTargetWord(GetSessionRules().ChoiceSide);
    ChosenOptionIndex = -1;

    BuildOptions();
    SpeakVoicePrompt();
}

void MultipleChoiceController::BuildOptions()
{
    Options.clear();
    CorrectOptionIndex = -1;

    if (!Session.has_value() || Session->IsFinished())
    {
        return;
    }

    const int CurrentIndex = Session->GetCurrentCardIndex();

    if (ContentBuilder)
    {
        BuildOptionsFromContent(CurrentIndex);

        return;
    }

    const std::string CorrectText = bShowWord ? Session->GetAnswer() : Session->GetPrompt();

    std::vector<std::string> Built;
    Built.push_back(CorrectText);

    std::vector<int> Candidates;
    Candidates.reserve(Session->GetCards().size());

    for (int Index = 0; Index < static_cast<int>(Session->GetCards().size()); ++Index)
    {
        if (Index != CurrentIndex)
        {
            Candidates.push_back(Index);
        }
    }

    std::shuffle(Candidates.begin(), Candidates.end(), OptionGenerator);

    for (const int Candidate : Candidates)
    {
        if (Built.size() >= ChoiceOptionCount)
        {
            break;
        }

        const std::string Text = bShowWord ? Session->GetAnswerFor(Candidate) : Session->GetPromptFor(Candidate);

        if (std::find(Built.begin(), Built.end(), Text) != Built.end())
        {
            continue;
        }

        Built.push_back(Text);
    }

    std::shuffle(Built.begin(), Built.end(), OptionGenerator);

    Options = Built;
    CorrectOptionIndex = static_cast<int>(std::find(Built.begin(), Built.end(), CorrectText) - Built.begin());
}

void MultipleChoiceController::BuildOptionsFromContent(int CurrentIndex)
{
    const FChoiceContent Content = ContentBuilder(*Session, CurrentIndex);

    ContentPrompt = Content.Prompt;
    ContentAnswerSpeech = Content.AnswerSpeech;
    bShowWord = Content.bShowsTargetLanguage;

    std::vector<std::string> Built{Content.CorrectOption};

    for (const std::string& Option : Content.Options)
    {
        if (Built.size() >= ChoiceOptionCount)
        {
            break;
        }

        if (std::find(Built.begin(), Built.end(), Option) == Built.end())
        {
            Built.push_back(Option);
        }
    }

    std::shuffle(Built.begin(), Built.end(), OptionGenerator);

    Options = Built;
    CorrectOptionIndex = static_cast<int>(std::find(Built.begin(), Built.end(), Content.CorrectOption) - Built.begin());
}

void MultipleChoiceController::SpeakVoicePrompt() const
{
    if (IsVoicePrompt())
    {
        Speak(GetPrompt());
    }
}

void MultipleChoiceController::SpeakWordAfterReveal()
{
    if (IsVoicePrompt() || !AppSpeech::Get().AllowsAutomaticSpeech())
    {
        return;
    }

    if (ShowsTargetLanguage())
    {
        SpeakPromptIfTargetLanguage();

        return;
    }

    if (CorrectOptionIndex < 0 || static_cast<size_t>(CorrectOptionIndex) >= Options.size())
    {
        return;
    }

    const std::string& Speech = ContentAnswerSpeech.empty() ? Options[static_cast<size_t>(CorrectOptionIndex)]
                                                            : ContentAnswerSpeech;

    Speak(QString::fromStdString(Speech));
}

QStringList MultipleChoiceController::GetOptionVerbGroups() const
{
    QStringList Result;
    for (const std::string& Option : Options)
    {
        QString Group;
        if (!bShowWord && !ContentBuilder && Session.has_value())
        {
            for (int Index = 0; Index < static_cast<int>(Session->GetCards().size()); ++Index)
            {
                if (Session->GetPromptFor(Index) == Option)
                {
                    Group = AppCards::GetVerbGroup(*Session->GetCards()[static_cast<size_t>(Index)]);
                    break;
                }
            }
        }
        Result << Group;
    }
    return Result;
}

QVariantList MultipleChoiceController::GetOptions() const
{
    QVariantList result;
    for (const auto& Option : Options)
    {
        result.append(GetShownText(QString::fromStdString(Option)));
    }
    return result;
}
int MultipleChoiceController::GetCorrectOptionIndex() const
{
    return CorrectOptionIndex;
}

int MultipleChoiceController::GetChosenOptionIndex() const
{
    return ChosenOptionIndex;
}

void MultipleChoiceController::choose(int OptionIndex)
{
    if (!Session.has_value() || bAnswered || OptionIndex < 0
        || static_cast<size_t>(OptionIndex) >= Options.size())
    {
        return;
    }

    RecordAnswer(OptionIndex == CorrectOptionIndex);

    ChosenOptionIndex = OptionIndex;

    qCDebug(LogSession) << metaObject()->className() << "answer:" << (WasLastAnswerCorrect() ? "correct" : "wrong")
                        << "- expected option" << CorrectOptionIndex << ", chosen option" << OptionIndex;

    SpeakWordAfterReveal();

    emit stateChanged();
}

void MultipleChoiceController::replay()
{
    SpeakPromptIfTargetLanguage();
}

void MultipleChoiceController::skip()
{
    if (!Session.has_value() || bAnswered)
    {
        return;
    }

    RecordSkip();

    ChosenOptionIndex = -1;

    qCDebug(LogSession) << metaObject()->className() << "answer: skipped (don't remember)";

    SpeakWordAfterReveal();

    emit stateChanged();
}

QString MultipleChoiceController::GetPrompt() const
{
    if (!Session.has_value())
    {
        return {};
    }

    if (ContentBuilder)
    {
        return QString::fromStdString(ContentPrompt);
    }

    return bShowWord ? QString::fromStdString(Session->GetPrompt()) : QString::fromStdString(Session->GetAnswer());
}

QString MultipleChoiceController::GetAnswer() const
{
    if (!Session.has_value())
    {
        return {};
    }

    if (ContentBuilder)
    {
        const bool bHasCorrectOption = CorrectOptionIndex >= 0 && static_cast<size_t>(CorrectOptionIndex) < Options.size();

        return bHasCorrectOption ? QString::fromStdString(Options[static_cast<size_t>(CorrectOptionIndex)]) : QString();
    }

    return bShowWord ? QString::fromStdString(Session->GetAnswer()) : QString::fromStdString(Session->GetPrompt());
}

bool MultipleChoiceController::ShowsTargetLanguage() const
{
    return bShowWord;
}

bool MultipleChoiceController::IsVoicePrompt() const
{
    return SupportsVoicePrompt() && bShowWord && ExerciseControllerBase::IsVoicePrompt();
}

bool MultipleChoiceController::SupportsVoicePrompt() const
{
    return !ContentBuilder;
}

QJsonObject MultipleChoiceController::SaveExtraState() const
{
    QJsonArray OptionsJson;

    for (const std::string& Option : Options)
    {
        OptionsJson.append(QString::fromStdString(Option));
    }

    QJsonObject Json;
    Json["showWord"] = bShowWord;
    Json["contentPrompt"] = QString::fromStdString(ContentPrompt);
    Json["contentAnswerSpeech"] = QString::fromStdString(ContentAnswerSpeech);
    Json["options"] = OptionsJson;
    Json["correctOptionIndex"] = CorrectOptionIndex;
    Json["chosenOptionIndex"] = ChosenOptionIndex;

    return Json;
}

void MultipleChoiceController::RestoreExtraState(const QJsonObject& Json)
{
    bShowWord = Json["showWord"].toBool(false);
    ContentPrompt = Json["contentPrompt"].toString().toStdString();
    ContentAnswerSpeech = Json["contentAnswerSpeech"].toString().toStdString();

    Options.clear();

    for (const QJsonValue& Option : Json["options"].toArray())
    {
        Options.push_back(Option.toString().toStdString());
    }

    CorrectOptionIndex = Json["correctOptionIndex"].toInt(-1);
    ChosenOptionIndex = Json["chosenOptionIndex"].toInt(-1);
}

void MultipleChoiceController::advance()
{
    if (!Session.has_value() || !bAnswered)
    {
        return;
    }

    Session->Advance();
    bShowWord = (SupportsVoicePrompt() && IsVoicePromptRequested()) || RollShowsTargetWord(GetSessionRules().ChoiceSide);
    ChosenOptionIndex = -1;
    bAnswered = false;

    if (!Session->IsFinished())
    {
        BuildOptions();
    }

    LogIfSessionFinished();

    SpeakVoicePrompt();

    emit stateChanged();
}
