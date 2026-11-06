#include "core/FTCardMeaning.h"

#include "core/FTAnswerComparison.h"

#include <algorithm>
#include <unordered_set>

namespace
{
    std::string MakeTranslationKey(const std::string& Translation)
    {
        FTAnswerComparisonRules KeepSpaces;
        KeepSpaces.bIgnoreWhitespace = false;

        return StripLeadingParticle(NormalizeAnswer(Translation, KeepSpaces));
    }

    std::optional<double> MeasureMeaningUse(const FTDictionarySense& Meaning)
    {
        std::optional<double> Total;

        for (const FTDictionaryTranslation& Translation : Meaning.Translations)
        {
            if (Translation.Probability.has_value())
            {
                Total = Total.value_or(0.0) + *Translation.Probability;
            }
        }

        return Total;
    }
}

bool MeaningFitsGender(const FTDictionarySense& Meaning, const std::optional<int>& Gender)
{
    return !Gender.has_value() || !Meaning.Gender.has_value() || Gender == Meaning.Gender;
}

const FTDictionarySense* FindCardMeaning(const FTCard& Card, const std::vector<FTDictionarySense>& Meanings)
{
    if (Card.MeaningGist.empty())
    {
        return nullptr;
    }

    for (const FTDictionarySense& Meaning : Meanings)
    {
        if (Meaning.Gist == Card.MeaningGist && MeaningFitsGender(Meaning, Card.Gender))
        {
            return &Meaning;
        }
    }

    return nullptr;
}

std::string MatchMeaningGist(const FTCard& Card, const std::vector<FTDictionarySense>& Meanings)
{
    std::unordered_set<std::string> CardKeys;

    for (const std::string& Part : SplitTranslations(Card.Translation))
    {
        CardKeys.insert(MakeTranslationKey(Part));
    }

    const FTDictionarySense* Matched = nullptr;

    for (const FTDictionarySense& Meaning : Meanings)
    {
        if (!MeaningFitsGender(Meaning, Card.Gender))
        {
            continue;
        }

        bool bShared = false;

        for (const FTDictionaryTranslation& Translation : Meaning.Translations)
        {
            if (CardKeys.count(MakeTranslationKey(Translation.Text)) > 0)
            {
                bShared = true;

                break;
            }
        }

        if (!bShared)
        {
            continue;
        }

        if (Matched != nullptr)
        {
            return {};
        }

        Matched = &Meaning;
    }

    return Matched != nullptr ? Matched->Gist : std::string();
}

std::string PickPopularMeaningGist(const FTCard& Card, const std::vector<FTDictionarySense>& Meanings)
{
    std::unordered_set<std::string> CardKeys;

    for (const std::string& Part : SplitTranslations(Card.Translation))
    {
        CardKeys.insert(MakeTranslationKey(Part));
    }

    const FTDictionarySense* Shared = nullptr;
    double SharedProbability = -1.0;
    const FTDictionarySense* Popular = nullptr;
    double PopularProbability = -1.0;

    for (const FTDictionarySense& Meaning : Meanings)
    {
        if (!MeaningFitsGender(Meaning, Card.Gender))
        {
            continue;
        }

        for (const FTDictionaryTranslation& Translation : Meaning.Translations)
        {
            const double Probability = Translation.Probability.value_or(0.0);

            if (Popular == nullptr || Probability > PopularProbability)
            {
                Popular = &Meaning;
                PopularProbability = Probability;
            }

            if (CardKeys.count(MakeTranslationKey(Translation.Text)) == 0)
            {
                continue;
            }

            if (Shared == nullptr || Probability > SharedProbability)
            {
                Shared = &Meaning;
                SharedProbability = Probability;
            }
        }
    }

    const FTDictionarySense* Picked = Shared != nullptr ? Shared : Popular;

    return Picked != nullptr ? Picked->Gist : std::string();
}

void SortMeaningsByUse(std::vector<FTDictionarySense>& Meanings)
{
    std::stable_sort(Meanings.begin(), Meanings.end(),
        [](const FTDictionarySense& Left, const FTDictionarySense& Right)
        {
            const std::optional<double> LeftUse = MeasureMeaningUse(Left);
            const std::optional<double> RightUse = MeasureMeaningUse(Right);

            if (LeftUse.has_value() != RightUse.has_value())
            {
                return LeftUse.has_value();
            }

            return LeftUse.value_or(0.0) > RightUse.value_or(0.0);
        });
}
