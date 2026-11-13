#include "core/FTDeckProgress.h"

#include "core/FTCard.h"
#include "core/FTTextNormalization.h"

#include <algorithm>


namespace
{
    bool IsFurther(const FTCard& Candidate, const FTCard& Current)
    {
        if (IsCompleted(Candidate) != IsCompleted(Current))
        {
            return IsCompleted(Candidate);
        }

        return Candidate.LeitnerStage > Current.LeitnerStage;
    }

    bool CoversMeaning(const std::string& MeaningGist, const std::string& RowGist)
    {
        return RowGist.empty() || MeaningGist.empty() || MeaningGist == RowGist;
    }
}

FTDeckProgressMatcher::FTDeckProgressMatcher(const std::vector<FTCardHandle>& Cards,
                                             const std::vector<FTKnownWord>& Known)
{
    for (const FTCardHandle& Card : Cards)
    {
        if (Card)
        {
            CardsByHeadword[FoldCase(Card->TargetWord)].push_back(Card);
        }
    }

    for (const FTKnownWord& Word : Known)
    {
        KnownMeanings[{FoldCase(Word.TargetWord), Word.PartOfSpeech}].insert(Word.MeaningGist);
    }
}

std::vector<FTDeckWordProgress> FTDeckProgressMatcher::Match(const std::vector<FTDeckWord>& Words) const
{
    std::vector<FTDeckWordProgress> Result;
    Result.reserve(Words.size());

    for (const FTDeckWord& Word : Words)
    {
        const std::string Headword = FoldCase(Word.Headword);

        FTDeckWordProgress Progress;

        const auto Found = CardsByHeadword.find(Headword);

        if (Found != CardsByHeadword.end())
        {
            for (const FTCardHandle& Card : Found->second)
            {
                if ((!Card->PartOfSpeech.empty() && Card->PartOfSpeech != Word.PartOfSpeech)
                    || !CoversMeaning(Card->MeaningGist, Word.Gist))
                {
                    continue;
                }

                if (!Progress.Card || IsFurther(*Card, *Progress.Card))
                {
                    Progress.Card = Card;
                }
            }
        }

        const auto Known = KnownMeanings.find({Headword, Word.PartOfSpeech});

        Progress.bKnown = !Progress.Card && Known != KnownMeanings.end()
            && std::any_of(Known->second.begin(), Known->second.end(),
                           [&Word](const std::string& MeaningGist) { return CoversMeaning(MeaningGist, Word.Gist); });

        Result.push_back(std::move(Progress));
    }

    return Result;
}

std::vector<FTDeckWordProgress> MatchDeckWords(const std::vector<FTDeckWord>& Words,
                                               const std::vector<FTCardHandle>& Cards,
                                               const std::vector<FTKnownWord>& Known)
{
    return FTDeckProgressMatcher(Cards, Known).Match(Words);
}
