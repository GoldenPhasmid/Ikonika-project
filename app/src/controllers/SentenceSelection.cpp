#include "SentenceSelection.h"

#include "AppDatabase.h"
#include "AppDictionary.h"
#include "SentenceTrainingController.h"

#include "core/FTCardRepository.h"
#include "core/FTDictionaryRepository.h"
#include "core/FTSentenceCards.h"
#include "core/FTSettings.h"
#include "core/FTWordForms.h"

#include <QString>

void AddPresentTenseKeys(const FTSentenceTree& Tree, const std::vector<uint32_t>& VerbWordIds, FTWordIdSet& Known)
{
    const int PresentTense = GetFormsTenseCode(FTSettings::Get().GetLanguageRules().TargetLanguage, FormsTensePresent);

    if (PresentTense <= 0 || Tree.GetWordSpan() == 0)
    {
        return;
    }

    for (const uint32_t WordId : VerbWordIds)
    {
        Known.Add(Tree.MakeTenseKey(WordId, static_cast<uint32_t>(PresentTense)));
    }
}

FTWordIdSet BuildKnownSentenceWords(const FTSentenceTree& Tree)
{
    const FTDictionaryRepository& Dictionary = AppDictionary::Get().GetRepository();
    const FTCardRepository& CardRepository = AppDatabase::Get().GetCardRepository();
    const std::string& TargetLanguage = FTSettings::Get().GetLanguageRules().TargetLanguage;

    FTWordIdSet Known = Dictionary.ResolveWordIds(CardRepository.ListKnownHeadwords());

    std::vector<uint32_t> PlacedVerbs;

    for (const FTRankedWord& Placed : Dictionary.ListWordsUpToTeachingRank(CardRepository.GetPlacementCutoff()))
    {
        Known.Add(Placed.WordId);

        if (Placed.bVerb)
        {
            PlacedVerbs.push_back(Placed.WordId);
        }
    }

    AddPresentTenseKeys(Tree, PlacedVerbs, Known);

    for (const FTKnownVerbTense& Verb : CardRepository.ListKnownVerbTenses())
    {
        const int Tense = GetFormsTenseCode(TargetLanguage, Verb.Tense);

        if (Tense <= 0 || Tree.GetWordSpan() == 0)
        {
            continue;
        }

        for (const uint32_t WordId : Dictionary.ListWordIds(Verb.Headword))
        {
            Known.Add(Tree.MakeTenseKey(WordId, static_cast<uint32_t>(Tense)));
        }
    }

    return Known;
}

const char* FindUnusableSentenceReason(const FTTreeSentence& Sentence)
{
    if (Sentence.Translation.empty())
    {
        return "no translation";
    }

    if (static_cast<int>(SplitSentenceWords(Sentence.Text).size()) < SentenceTrainingController::MinimumSentenceWords)
    {
        return "too few words";
    }

    if (QString::fromStdString(Sentence.Text).size() > SentenceTrainingController::MaximumSentenceCharacters)
    {
        return "too long";
    }

    return nullptr;
}
