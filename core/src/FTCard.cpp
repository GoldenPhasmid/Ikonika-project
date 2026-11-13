#include "core/FTCard.h"

#include "core/FTArticleRules.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string_view>

static bool IsEnglishModalVerb(const std::string& Word)
{
    static constexpr std::array<std::string_view, 9> ModalVerbs{
        "can", "could", "may", "might", "must", "shall", "should", "will", "would"};

    return std::any_of(ModalVerbs.begin(), ModalVerbs.end(), [&](std::string_view ModalVerb)
    {
        return std::equal(Word.begin(), Word.end(), ModalVerb.begin(), ModalVerb.end(),
            [](unsigned char Letter, char ModalLetter)
            {
                return std::tolower(Letter) == ModalLetter;
            });
    });
}

bool IsUserEdited(const FTCard& Subject, EFTCardField Field)
{
    return (Subject.UserEditedFields & static_cast<uint32_t>(Field)) != 0;
}

void SetUserEdited(FTCard& Subject, EFTCardField Field, bool bEdited)
{
    if (bEdited)
    {
        Subject.UserEditedFields |= static_cast<uint32_t>(Field);

        return;
    }

    Subject.UserEditedFields &= ~static_cast<uint32_t>(Field);
}

bool IsInPool(const FTCard& Subject)
{
    return Subject.LeitnerStage == PoolStage && !Subject.CompletedDate.has_value();
}

bool IsCompleted(const FTCard& Subject)
{
    return Subject.CompletedDate.has_value();
}

std::string GetDisplayWord(const FTCard& Subject, const std::string& TargetLanguage)
{
    if (!Subject.Gender.has_value() || Subject.PartOfSpeech != "noun")
    {
        return GetDisplayHeadword(Subject.TargetWord, Subject.PartOfSpeech, TargetLanguage);
    }

    const FTArticleRules* Rules = FindArticleRules(TargetLanguage);

    return Rules != nullptr ? Rules->GetDisplayWord(Subject) : Subject.TargetWord;
}

std::string GetDisplayHeadword(const std::string& Headword, const std::string& PartOfSpeech,
                               const std::string& TargetLanguage)
{
    if (Headword.empty() || TargetLanguage != "en" || PartOfSpeech != "verb" || IsEnglishModalVerb(Headword))
    {
        return Headword;
    }

    if (Headword.size() >= 3 && (Headword[0] == 't' || Headword[0] == 'T')
        && (Headword[1] == 'o' || Headword[1] == 'O') && Headword[2] == ' ')
    {
        return Headword;
    }

    return "to " + Headword;
}

static std::string PrefixEachTranslation(const std::string& Translations, const std::string& Prefix)
{
    std::string Result;

    size_t Start = 0;

    while (Start <= Translations.size())
    {
        const size_t Comma = Translations.find(',', Start);
        const size_t End = Comma == std::string::npos ? Translations.size() : Comma;

        size_t ItemStart = Start;
        size_t ItemEnd = End;

        while (ItemStart < ItemEnd && Translations[ItemStart] == ' ')
        {
            ++ItemStart;
        }

        while (ItemEnd > ItemStart && Translations[ItemEnd - 1] == ' ')
        {
            --ItemEnd;
        }

        if (ItemEnd > ItemStart)
        {
            const std::string Item = Translations.substr(ItemStart, ItemEnd - ItemStart);

            if (!Result.empty())
            {
                Result += ", ";
            }

            Result += Prefix == "to " ? GetDisplayHeadword(Item, "verb", "en")
                : Item.rfind(Prefix, 0) == 0 ? Item : Prefix + Item;
        }

        if (Comma == std::string::npos)
        {
            break;
        }

        Start = Comma + 1;
    }

    return Result.empty() ? Translations : Result;
}

std::string GetDisplayTranslation(const FTCard& Subject, const std::string& NativeLanguage)
{
    if (Subject.Translation.empty() || NativeLanguage != "en")
    {
        return Subject.Translation;
    }

    if (Subject.PartOfSpeech == "verb")
    {
        return PrefixEachTranslation(Subject.Translation, "to ");
    }

    if (Subject.PartOfSpeech == "noun" && Subject.bSpellingIsAlsoAVerb)
    {
        return PrefixEachTranslation(Subject.Translation, "the ");
    }

    return Subject.Translation;
}

std::string GetGenderCode(const std::optional<int>& Gender)
{
    if (!Gender.has_value())
    {
        return {};
    }

    if (*Gender == GenderCommon)
    {
        return "m/f";
    }

    if (*Gender == GenderNeuter)
    {
        return "n";
    }

    return *Gender == GenderMasculine ? "m" : "f";
}
