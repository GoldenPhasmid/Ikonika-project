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

namespace
{
    constexpr int GrammarVerbCount = 10;
}

void AddPresentTenseKeys(const FTSentenceTree& Tree, const std::vector<uint32_t>& VerbWordIds, FTWordIdSet& Known,
                         const std::string& TargetLanguage)
{
    const int PresentTense = GetFormsTenseCode(TargetLanguage, FormsTensePresent);

    if (PresentTense <= 0 || Tree.GetWordSpan() == 0)
    {
        return;
    }

    for (const uint32_t WordId : VerbWordIds)
    {
        Known.Add(Tree.MakeTenseKey(WordId, static_cast<uint32_t>(PresentTense)));
    }
}

void AddPresentTenseKeys(const FTSentenceTree& Tree, const std::vector<uint32_t>& VerbWordIds, FTWordIdSet& Known)
{
    AddPresentTenseKeys(Tree, VerbWordIds, Known, FTSettings::Get().GetLanguageRules().TargetLanguage);
}

FSentenceSources MakeSentenceSources()
{
    const std::string& TargetLanguage = FTSettings::Get().GetLanguageRules().TargetLanguage;

    return FSentenceSources{&AppDictionary::Get().GetRepository(), &AppDatabase::Get().GetCardRepository(),
                            TargetLanguage, FTSettings::Get().GetEssentialVerbs(TargetLanguage)};
}

FTWordIdSet BuildKnownSentenceWords(const FTSentenceTree& Tree, const FSentenceSources& Sources)
{
    const FTDictionaryRepository& Dictionary = *Sources.Dictionary;
    const FTCardRepository& CardRepository = *Sources.Cards;
    const std::string& TargetLanguage = Sources.TargetLanguage;

    FTWordIdSet Known = Dictionary.ResolveWordIds(CardRepository.ListKnownHeadwords());

    std::vector<uint32_t> PlacedVerbs;

    std::vector<FTRankedWord> Given = Dictionary.ListWordsUpToTeachingRank(CardRepository.GetPlacementCutoff());
    const std::vector<FTRankedWord> Grammar = Dictionary.ListGrammarWords(Sources.EssentialVerbs, GrammarVerbCount);
    Given.insert(Given.end(), Grammar.begin(), Grammar.end());

    for (const FTRankedWord& Placed : Given)
    {
        Known.Add(Placed.WordId);

        if (Placed.bVerb)
        {
            PlacedVerbs.push_back(Placed.WordId);
        }
    }

    AddPresentTenseKeys(Tree, PlacedVerbs, Known, TargetLanguage);

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

FTWordIdSet BuildKnownSentenceWords(const FTSentenceTree& Tree)
{
    return BuildKnownSentenceWords(Tree, MakeSentenceSources());
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
