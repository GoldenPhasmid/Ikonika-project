#include "GapProductionController.h"

#include "core/FTCard.h"
#include "core/FTSentenceCards.h"
#include "core/FTWordForms.h"

#include <QCoreApplication>

FGapSentenceLookup MakeGapSentenceLookup(std::shared_ptr<const FGapSentences> Sentences)
{
    return [Sentences = std::move(Sentences)](const FTCard& Card) -> const FTTreeSentence*
    {
        const auto Found = Sentences->find(Card.Id);

        return Found != Sentences->end() ? &Found->second : nullptr;
    };
}

GapProductionController::GapProductionController(QObject* Parent)
    : WrittenProductionController(Parent)
{
    SetAskWord(false);
}

void GapProductionController::SetSentenceLookup(FGapSentenceLookup InSentenceLookup)
{
    SentenceLookup = std::move(InSentenceLookup);
}

const FTTreeSentence* GapProductionController::FindSentence(const FTCard& Card) const
{
    return SentenceLookup ? SentenceLookup(Card) : nullptr;
}

const FTTreeSentence* GapProductionController::FindCurrentSentence() const
{
    const FTCard* Current = GetCurrentCard();

    return Current != nullptr ? FindSentence(*Current) : nullptr;
}

WordGapController::WordGapController(QObject* Parent)
    : GapProductionController(Parent)
{
    AnswerHint = QCoreApplication::translate("FMasteryLoopStage", "Type the missing word...");
}

std::optional<FWrittenContent> WordGapController::BuildCardContent(const FTCard& Card) const
{
    const FTTreeSentence* Sentence = FindSentence(Card);

    if (Sentence == nullptr)
    {
        return std::nullopt;
    }

    FWrittenContent Content;
    Content.Heading = Sentence->Translation;
    Content.Prompt = MakeGapPrompt(*Sentence);
    Content.AcceptedAnswers = {GetGapWord(*Sentence)};
    Content.Translation = GetDisplayTranslation(Card, Params.NativeLanguage);
    Content.RevealedAnswer = Sentence->Text;
    Content.bSentence = true;

    return Content;
}

FExerciseSpeech WordGapController::GetHiddenSpeech() const
{
    const FTTreeSentence* Sentence = FindCurrentSentence();

    if (Sentence == nullptr)
    {
        return GapProductionController::GetHiddenSpeech();
    }

    return FExerciseSpeech{QString::fromStdString(MakeGapSpeech(*Sentence)), true};
}

FExerciseSpeech WordGapController::GetRevealSpeech() const
{
    const FTTreeSentence* Sentence = FindCurrentSentence();

    if (Sentence == nullptr)
    {
        return GapProductionController::GetRevealSpeech();
    }

    return FExerciseSpeech{QString::fromStdString(GetGapWord(*Sentence))};
}

SentenceGapController::SentenceGapController(QObject* Parent)
    : GapProductionController(Parent)
{
    AnswerHint = QCoreApplication::translate("SentenceTrainingController", "Type the missing word...");
}

std::optional<FWrittenContent> SentenceGapController::BuildCardContent(const FTCard& Card) const
{
    const FTTreeSentence* Sentence = FindSentence(Card);

    if (Sentence == nullptr)
    {
        return std::nullopt;
    }

    FWrittenContent Content;
    Content.Prompt = MakeGapPrompt(*Sentence);
    Content.AcceptedAnswers = {GetGapWord(*Sentence)};
    Content.bSentence = true;

    return Content;
}

FExerciseSpeech SentenceGapController::GetHiddenSpeech() const
{
    const FTTreeSentence* Sentence = FindCurrentSentence();

    if (Sentence == nullptr)
    {
        return GapProductionController::GetHiddenSpeech();
    }

    return FExerciseSpeech{QString::fromStdString(Sentence->Text), true};
}

FExerciseSpeech SentenceGapController::GetRevealSpeech() const
{
    if (FindCurrentSentence() == nullptr)
    {
        return GapProductionController::GetRevealSpeech();
    }

    return {};
}

FormGapController::FormGapController(QObject* Parent)
    : GapProductionController(Parent)
{
    AnswerHint = QCoreApplication::translate("FormsStages", "Type the form...");
}

std::optional<FWrittenContent> FormGapController::BuildCardContent(const FTCard& Card) const
{
    FWrittenContent Content;

    const std::optional<FTWordForm> Read = ReadWordForm(Card);
    const FTTreeSentence* Sentence = FindSentence(Card);

    if (Sentence != nullptr)
    {
        Content.Heading = Read.has_value() ? Read->Headword : std::string();
        Content.Prompt = MakeGapPrompt(*Sentence);
        Content.AcceptedAnswers = ListAcceptedForms(Card);
        Content.AcceptedAnswers.push_back(GetGapWord(*Sentence));
        Content.Translation = Sentence->Translation;
        Content.RevealedAnswer = Sentence->Text;
        Content.bSentence = true;

        return Content;
    }

    if (!Read.has_value())
    {
        Content.Prompt = Card.Translation;
        Content.AcceptedAnswers = {Card.TargetWord};

        return Content;
    }

    Content.Heading = Read->Headword;
    Content.Prompt = MakeFormPrompt(Read->SubjectLabel);
    Content.AcceptedAnswers = ListAcceptedForms(Card);
    Content.Translation = Read->Meaning;

    return Content;
}

FExerciseSpeech FormGapController::GetRevealSpeech() const
{
    const FTTreeSentence* Sentence = FindCurrentSentence();

    if (Sentence == nullptr)
    {
        return GapProductionController::GetRevealSpeech();
    }

    return FExerciseSpeech{QString::fromStdString(Sentence->Text), true};
}
