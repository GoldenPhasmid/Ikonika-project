#include "core/FTArticleRules.h"

#include "core/FTCard.h"

#include <initializer_list>

namespace
{
    const char* const PrimaryStressMark = "ˈ";

    bool StartsWithAny(const std::string& Word, std::initializer_list<const char*> Prefixes)
    {
        for (const char* Prefix : Prefixes)
        {
            if (Word.rfind(Prefix, 0) == 0)
            {
                return true;
            }
        }

        return false;
    }

    bool StartsWithFrenchVowelSound(const std::string& Word)
    {
        return StartsWithAny(Word, {
            "a", "e", "i", "o", "u", "y", "A", "E", "I", "O", "U", "Y",
            "à", "â", "ä", "é", "è", "ê", "ë", "î", "ï", "ô", "ö", "ù", "û", "ü",
            "À", "Â", "Ä", "É", "È", "Ê", "Ë", "Î", "Ï", "Ô", "Ö", "Ù", "Û", "Ü",
        });
    }

    bool StartsWithSpanishA(const std::string& Word)
    {
        return StartsWithAny(Word, {"a", "á", "A", "Á", "ha", "há", "Ha", "Há"});
    }

    class FTFrenchArticleRules : public FTArticleRules
    {
    public:
        std::string GetDisplayWord(const FTCard& Subject) const override
        {
            if (Subject.bElidesArticle || StartsWithFrenchVowelSound(Subject.TargetWord))
            {
                return "l'" + Subject.TargetWord;
            }

            return (*Subject.Gender == GenderFeminine ? "la " : "le ") + Subject.TargetWord;
        }
    };

    class FTSpanishArticleRules : public FTArticleRules
    {
    public:
        std::string GetDisplayWord(const FTCard& Subject) const override
        {
            if (*Subject.Gender != GenderFeminine)
            {
                return "el " + Subject.TargetWord;
            }

            if (Subject.bStressedFirstSyllable && StartsWithSpanishA(Subject.TargetWord))
            {
                return "el " + Subject.TargetWord;
            }

            return "la " + Subject.TargetWord;
        }
    };

    class FTPortugueseArticleRules : public FTArticleRules
    {
    public:
        std::string GetDisplayWord(const FTCard& Subject) const override
        {
            return (*Subject.Gender == GenderFeminine ? "a " : "o ") + Subject.TargetWord;
        }
    };

    bool StartsWithItalianVowel(const std::string& Word)
    {
        static const std::initializer_list<const char*> Vowels = {
            "a", "e", "i", "o", "u", "à", "è", "é", "ì", "ò", "ù", "h",
        };

        if (!StartsWithAny(Word, Vowels))
        {
            return false;
        }

        const bool bSemivowelI = Word.size() > 1 && Word[0] == 'i' && StartsWithAny(Word.substr(1), Vowels);

        return !bSemivowelI;
    }

    bool TakesItalianLo(const std::string& Word)
    {
        if (StartsWithAny(Word, {"z", "x", "y", "gn", "ps", "pn"}) || (Word.size() > 1 && Word[0] == 'i'))
        {
            return true;
        }

        return Word.size() > 1 && Word[0] == 's' && !StartsWithItalianVowel(Word.substr(1));
    }

    class FTItalianArticleRules : public FTArticleRules
    {
    public:
        std::string GetDisplayWord(const FTCard& Subject) const override
        {
            if (StartsWithItalianVowel(Subject.TargetWord))
            {
                return "l'" + Subject.TargetWord;
            }

            if (*Subject.Gender == GenderFeminine)
            {
                return "la " + Subject.TargetWord;
            }

            return (TakesItalianLo(Subject.TargetWord) ? "lo " : "il ") + Subject.TargetWord;
        }
    };

    const FTFrenchArticleRules FrenchRules;
    const FTSpanishArticleRules SpanishRules;
    const FTPortugueseArticleRules PortugueseRules;
    const FTItalianArticleRules ItalianRules;
}

const FTArticleRules* FindArticleRules(const std::string& TargetLanguage)
{
    if (TargetLanguage == "fr")
    {
        return &FrenchRules;
    }

    if (TargetLanguage == "es")
    {
        return &SpanishRules;
    }

    if (TargetLanguage == "pt")
    {
        return &PortugueseRules;
    }

    if (TargetLanguage == "it")
    {
        return &ItalianRules;
    }

    return nullptr;
}

bool IsFirstSyllableStressed(const std::string& Phonetic)
{
    const std::string::size_type Mark = Phonetic.find(PrimaryStressMark);

    if (Mark == std::string::npos)
    {
        return false;
    }

    static const std::initializer_list<const char*> Vowels = {
        "a", "e", "i", "o", "u", "y",
        "ɑ", "ɐ", "ɛ", "ə", "ɪ", "ɔ", "ø", "œ", "ʊ", "ʌ", "æ", "ɜ", "ɨ", "ɯ", "ɵ",
    };

    for (std::string::size_type Index = 0; Index < Mark; ++Index)
    {
        for (const char* Vowel : Vowels)
        {
            if (Phonetic.compare(Index, std::string(Vowel).size(), Vowel) == 0)
            {
                return false;
            }
        }
    }

    return true;
}
